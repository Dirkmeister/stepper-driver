/* ======================================================================
   Universal Stepper Tester   firmware v1.0   (2026-10-12)   -DK
   ======================================================================
   Board   : Universal_motor_driver_v09 main board + front UI board
   MCU     : ATmega328P, 16 MHz crystal, programmed over ISP, no bootloader
   Driver  : BTT TMC2209 V1.2, UART on PD0/PD1 (R16 1k on TX), address 0
   Display : SSD1306 128x64 I2C OLED over the J10/J11 ribbon
   Libraries: none. The OLED, I2C and TMC2209 UART code are all built in,
             so the self test can say exactly where a fault is and nothing
             can hang the loop. (U8g2 and TMCStepper are not used.)

   Arduino IDE, Boards Manager: "MiniCore" by MCUdude
     Tools: Board ATmega328 | Clock External 16 MHz | BOD 2.7V
            EEPROM retained | Compiler LTO enabled | Variant 328P / 328PA
            Bootloader: No bootloader | Programmer: Arduino as ISP
     Programmer = Uno running File > Examples > 11.ArduinoISP, with a
     10 uF cap from the Uno's RESET to GND.
     Once per chip : Tools > Burn Bootloader   (only writes fuses F7 D7 FD)
     Every update  : Sketch > Upload Using Programmer

   Controls
     turn         speed (main screen), move or change a value (menus)
     push + turn  fine speed (about 2 % per click)
     press        run / stop (main), select (menus), clear a fault
     hold         menu (main), back (menus).  Hold = 0.6 s
     Hold the knob while powering up to restore default settings.

   Self test (every power-up, about 1.5 s; turn the knob to scroll, the
   hint for the highlighted line shows at the bottom)
      1 CLK   CPU clock vs the watchdog RC: catches unset fuses (1 or 8 MHz)
      2 5V    5 V rail via the internal 1.1 V bandgap (+-10 %)
      3 VM    motor supply through the 10k / 1.8k divider
      4 OLED  SDA/SCL idle high, ACK at 0x3C or 0x3D, 400 kHz then 100 kHz
      5 KNOB  button released
      6 UART  echo through R16: proves PD1, R16, PD0 and the PDN net
      7 LINK  driver answers (scans addresses 1-3 if not at 0)
      8 DRV   version 0x21, IFCNT counts every write, config reads back
      9 PINS  EN, DIR, STEP toggled and read back through IOIN
     10 DIAG  PB0 pulled up, driver holds it low: wired or not (optional)
     11 OUT   outputs on at 306 mA for 80 ms, short-to-GND / supply latches
   Tests 7-11 need 12 V on VM. A self test that ran without it re-runs by
   itself once VM shows up.

   Front STATUS LED
     solid            self test running
     N blinks, pause  self test failed, N = number of the first failed test
     blip every 2 s   idle, all good
     on               motor running
     fast flicker     fault (press the knob to clear)

   Debug log (optional): J9 pin 1 (PB2) to a USB-serial RX, J9 pin 6 GND,
   57600 8N1. No adapter? A Nano with RESET jumpered to GND works: PB2 to
   the Nano's TX pin (D1), GNDs together, Serial Monitor on the Nano's port.
   ====================================================================== */

#include <Arduino.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/boot.h>
#include <util/delay.h>
#include <stdarg.h>
#include <stdio.h>

#define FW_VER "1.0"

// ---------------------------------------------------------------- pins
// PORTD: PD0 RXD + PD1 TXD (driver UART), PD2 ENC_A, PD3 ENC_B, PD4 ENC_SW,
//        PD5 status LED, PD6 DIR, PD7 EN
// PORTB: PB0 DIAG (J7), PB1 STEP (OC1A), PB2 debug TX (J9-1), PB3..5 ISP only
// PORTC: PC0 VM sense (10k/1.8k), PC1 buzzer (DNP), PC2/PC3 spare (J9),
//        PC4 SDA, PC5 SCL
#define EN_OFF()   (PORTD |=  _BV(PD7))   // high = driver off (R8 holds this during reset)
#define EN_ON()    (PORTD &= ~_BV(PD7))   // low  = driver on, blue EN LED (D14) lights
#define DIR_HI()   (PORTD |=  _BV(PD6))
#define DIR_LO()   (PORTD &= ~_BV(PD6))
#define LED_ON()   (PORTD |=  _BV(PD5))
#define LED_OFF()  (PORTD &= ~_BV(PD5))
#define SW_DOWN()  (!(PIND & _BV(PD4)))
#define DIAG_HI()  (PINB & _BV(PB0))

// ---------------------------------------------------------------- limits
#define VM_ABSENT_MV  4000    // below this: no motor supply at all
#define VM_OK_MV      9000    // self test wants at least this
#define VM_RUN_MV     8000    // running below this trips a fault
#define VM_HIGH_MV   16000    // above this: not a 12 V brick (D1 is a 15 V zener)
#define VM_MAX_MV    25000    // C1 / C14 are 25 V parts
#define LONG_MS        600    // long press
#define MAX_CRPM    200000L   // 2000 rpm cap, speeds are in 0.01 rpm

// ---------------------------------------------------------------- TMC2209 registers
#define R_GCONF      0x00
#define R_GSTAT      0x01
#define R_IFCNT      0x02
#define R_IOIN       0x06
#define R_IHOLD_IRUN 0x10
#define R_TPOWERDOWN 0x11
#define R_TSTEP      0x12
#define R_TPWMTHRS   0x13
#define R_TCOOLTHRS  0x14
#define R_VACTUAL    0x22
#define R_SGTHRS     0x40
#define R_SG_RESULT  0x41
#define R_COOLCONF   0x42
#define R_MSCNT      0x6A
#define R_CHOPCONF   0x6C
#define R_DRV_STATUS 0x6F
#define R_PWMCONF    0x70
#define R_PWM_SCALE  0x71
#define R_PWM_AUTO   0x72

// DRV_STATUS bits
#define DS_OTPW  0x00000001UL
#define DS_OT    0x00000002UL
#define DS_OLA   0x00000040UL
#define DS_OLB   0x00000080UL
#define DS_SHORT 0x0000003CUL     // s2ga s2gb s2vsa s2vsb

// PWMCONF: ofs 36, grad 14, freq 1, autoscale, autograd, reg 8, lim 12
// (the TMC2209 values Klipper ships on thousands of printers)
#define PWMCONF_VAL 0xC80D0E24UL

// UART link error codes
enum { TE_OK = 0, TE_NOREPLY, TE_PARTIAL, TE_CRC, TE_HDR };

// ---------------------------------------------------------------- types
struct Settings {
  uint8_t  magic, ver;
  uint16_t mA;        // requested run current, mA RMS, 100..1750
  uint8_t  mres;      // microsteps = 1 << mres (0 full step .. 8 = 1/256)
  uint8_t  spread;    // 0 StealthChop, 1 SpreadCycle
  uint8_t  stepDrv;   // 0 speed over UART (VACTUAL), 1 STEP pin pulses (also J5)
  uint8_t  spr400;    // 0 = 200 steps/rev (1.8 deg), 1 = 400 (0.9 deg)
  uint8_t  accel;     // index into ACCEL_TAB
  uint8_t  stall;     // 0 off, else stop when SG_RESULT < this (StealthChop)
  uint8_t  knobRev;   // flip the knob direction
  uint8_t  flip;      // rotate the screen 180 deg
  int8_t   trim;      // VM reading trim, 0.1 % steps
  uint8_t  log;       // 1 Hz status line on the debug port
  int32_t  speed;     // speed setpoint, 0.01 rpm, sign = direction
  uint8_t  sum;
};

enum { T_CLK, T_5V, T_VM, T_OLED, T_KNOB, T_UART, T_LINK, T_DRV, T_PINS, T_DIAG, T_OUT, T_N };
enum { R_NONE, R_OK, R_WARN, R_FAIL, R_SKIP };
enum { SC_POST, SC_MAIN, SC_MENU, SC_COIL, SC_REGS };
enum { F_NONE, F_OT, F_SHORT, F_VM, F_LINK, F_STALL, F_DRV };
enum { M_OFF, M_START, M_RUN, M_STOP };
enum { MI_CURR, MI_USTEP, MI_CHOP, MI_DRIVE, MI_ACCEL, MI_SPR, MI_STALL, MI_KNOB, MI_FLIP,
       MI_TRIM, MI_LOG, MI_COIL, MI_REGS, MI_POST, MI_DEF, MI_EXIT, MI_N };

// ---------------------------------------------------------------- globals
static Settings S;
static const uint16_t ACCEL_TAB[] PROGMEM = {0, 10, 25, 50, 100, 200, 500, 1000, 2000};   // rpm/s
#define N_ACCEL 9
// coarse knob speeds, 0.1 rpm units
static const uint16_t NICE[] PROGMEM = {5, 10, 20, 50, 100, 150, 200, 300, 450, 600, 900, 1200, 1500,
                                        1800, 2400, 3000, 3600, 4200, 4800, 6000, 7200, 9000, 12000,
                                        15000, 20000};
#define N_NICE 25

uint8_t resetCause __attribute__((section(".noinit")));

// inputs (ISR side)
static volatile int8_t   encDet;
static volatile uint8_t  btnEvt, btnDown, btnTurned;
static volatile uint16_t btnMs;
static uint8_t encPrev, encRest;
static int8_t  encAcc;

// driver
static uint8_t  tmcAddr;            // UART address in use (0 unless MS1/MS2 are wrong)
static bool     tmcOk;              // link up and configured
static bool     spreadPin;          // SPREAD pin level seen in IOIN (inverts en_SpreadCycle)
static bool     diagWired;          // DIAG reaches PB0
static uint8_t  echoState;          // last UART echo: 0 ok, 1 none, 2 garbled, 3 line low
static uint8_t  gCS, gVs;           // current scale and vsense for S.mA
static uint32_t drvStatus;
static uint16_t sgResult;
static uint8_t  pwmSum;
static uint8_t  linkFails, drvResets;
static int32_t  vactNow;

// power
static uint16_t vmMv, vccMv;
static bool     vmWasAbsent;
static uint8_t  vmSeen;

// motion
static uint8_t  mState;
static int32_t  spNow;              // ramp output, 0.01 rpm
static uint32_t mT, rampT, steadyT;
static uint8_t  fault;
static uint8_t  sgLowCount;

// STEP timer (Timer1, OC1A = PB1)
static volatile uint16_t t1Top, t1Ocr;
static volatile uint8_t  t1Cs, t1Pend;
static uint8_t  stepOn;
static bool     stepNeg;

// self test
static uint8_t  tRes[T_N], tCode[T_N];
static int16_t  tVal[T_N];
static uint8_t  postFirstFail;      // 1-based, 0 = none
static uint8_t  lfuse, hfuse, efuse;
static uint16_t clkTicks;

// coil check
static uint8_t  coilN, coilA, coilB, coilShort, coilOt;
static uint16_t coilmA;

// UI
static bool     oledOk;
static uint8_t  oledAddr, oledFast, oledErrs;
static uint8_t  screen, cur, top, editing;
static PGM_P    toastMsg;
static uint32_t toastUntil, tDraw;
static uint32_t regVal[12];
static uint8_t  regOk;

// text screen: 8 rows x 21 columns of 6x8 cells, optional 2x row pair
static char     tb[8][21];
static uint32_t tinv[8];            // bit c = invert column c, bit 31 = invert the 2 px margin
static uint8_t  bigRow = 0xFF, bigX;
static char     bigS[9];
static uint16_t rowSum[8];

// ================================================================ early init
// Runs before main(): save the reset cause and stop a watchdog left running.
void earlyInit(void) __attribute__((naked, used, section(".init3")));
void earlyInit(void) {
  resetCause = MCUSR;
  MCUSR = 0;
  wdt_disable();
}

// ================================================================ debug log (J9-1)
// TX-only 57600 8N1, bit-banged with interrupts off for one byte (~174 us).
#define DBG_CYC (F_CPU / 57600UL)
void dbgTx(uint8_t c) {
  uint8_t s = SREG;
  cli();
  PORTB &= ~_BV(PB2);                                   // start bit
  __builtin_avr_delay_cycles(DBG_CYC - 7);
  for (uint8_t i = 0; i < 8; i++) {
    if (c & 1) PORTB |= _BV(PB2); else PORTB &= ~_BV(PB2);
    c >>= 1;
    __builtin_avr_delay_cycles(DBG_CYC - 13);
  }
  __builtin_avr_delay_cycles(6);
  PORTB |= _BV(PB2);                                    // stop bit
  SREG = s;
  __builtin_avr_delay_cycles(DBG_CYC);
}

void dbgStr(const char* p) {
  while (*p) { if (*p == '\n') dbgTx('\r'); dbgTx(*p++); }
}

void dbgf(PGM_P fmt, ...) {
  char b[80];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf_P(b, sizeof b, fmt, ap);
  va_end(ap);
  dbgStr(b);
}
#define DBG(f, ...) dbgf(PSTR(f), ##__VA_ARGS__)

// ================================================================ settings
uint8_t setSum(const Settings* s) {
  const uint8_t* p = (const uint8_t*)s;
  uint8_t x = 0xA5;
  for (uint8_t i = 0; i < sizeof(Settings) - 1; i++) x += p[i];
  return x;
}

void setDefaults() {
  memset(&S, 0, sizeof S);
  S.magic = 0x5A; S.ver = 1;
  S.mA = 400;     S.mres = 4;      // 400 mA, 1/16
  S.accel = 5;    S.speed = 6000;  // 200 rpm/s, 60 rpm
}

bool loadSettings() {
  eeprom_read_block(&S, (const void*)0, sizeof S);
  if (S.magic != 0x5A || S.ver != 1 || S.sum != setSum(&S) || S.mA < 100 || S.mA > 1750 ||
      S.mres > 8 || S.accel >= N_ACCEL || S.speed > MAX_CRPM || S.speed < -MAX_CRPM ||
      S.trim < -100 || S.trim > 100) {
    setDefaults();
    return false;
  }
  return true;
}

void saveSettings() {
  S.sum = setSum(&S);
  eeprom_update_block(&S, (void*)0, sizeof S);          // only rewrites changed bytes
}

// ================================================================ inputs
// 1 kHz tick: debounced knob button with short / long events. A press that
// had the knob turned during it (push + turn) gives no event.
ISR(TIMER2_COMPA_vect) {
  static uint8_t h;
  h = (uint8_t)(h << 1) | (SW_DOWN() ? 1 : 0);
  if (h == 0xFF) btnDown = 1;
  else if (h == 0x00) btnDown = 0;
  if (btnDown) {
    if (btnMs < 60000) btnMs++;
    if (btnMs == LONG_MS && !btnTurned) btnEvt = 2;
  } else {
    if (btnMs >= 15 && btnMs < LONG_MS && !btnTurned) btnEvt = 1;
    btnMs = 0;
    btnTurned = 0;
  }
}

// Encoder: full quadrature state machine on both edges of A and B. One count
// per detent, emitted when the encoder settles back in its rest state, so
// bounce or a missed edge can never leave it half a click off.
static const int8_t QEM[16] PROGMEM = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
ISR(INT0_vect) {
  uint8_t s = (PIND >> PD2) & 3;                       // bit0 = A (PD2), bit1 = B (PD3)
  encAcc += (int8_t)pgm_read_byte(&QEM[(encPrev << 2) | s]);
  encPrev = s;
  if (s == encRest) {
    int8_t d = encAcc <= -2 ? 1 : (encAcc >= 2 ? -1 : 0);   // A on PD2: clockwise counts negative
    encDet += d;
    if (d && btnDown) btnTurned = 1;
    encAcc = 0;
  }
}
ISR(INT1_vect, ISR_ALIASOF(INT0_vect));

int8_t takeEnc() {
  cli();
  int8_t d = encDet;
  encDet = 0;
  sei();
  return S.knobRev ? -d : d;
}

uint8_t takeBtn() {
  cli();
  uint8_t e = btnEvt;
  btnEvt = 0;
  sei();
  return e;
}

void inputsInit() {
  TCCR2A = _BV(WGM21);                                 // Timer2 CTC, clk/64, 250 counts = 1 kHz
  TCCR2B = _BV(CS22);
  OCR2A = 249;
  TIMSK2 = _BV(OCIE2A);
  encPrev = encRest = (PIND >> PD2) & 3;              // the knob sits in a detent at power-up
  EICRA = _BV(ISC00) | _BV(ISC10);                     // any edge on INT0 and INT1
  EIFR = 3;
  EIMSK = _BV(INT0) | _BV(INT1);
}

// ================================================================ ADC
uint16_t adcSum(uint8_t mux, uint8_t n) {
  ADMUX = _BV(REFS0) | mux;                            // AVCC reference
  if (mux == 0x0E) delay(2);                           // bandgap needs time to settle
  uint16_t s = 0;
  for (int8_t i = -1; i < (int8_t)n; i++) {            // first conversion thrown away
    ADCSRA |= _BV(ADSC);
    while (ADCSRA & _BV(ADSC)) { }
    if (i >= 0) s += ADC;
  }
  return s;
}

uint16_t readVM() {
  // VM = adc * 5000/1023 * (10k + 1.8k)/1.8k = adc * 32.041 mV, 16 samples
  uint32_t mv = (uint32_t)adcSum(0, 16) * 32041UL / 16000UL;
  mv = mv * (uint32_t)(1000 + S.trim) / 1000UL;
  return (uint16_t)mv;
}

uint16_t readVcc() {
  // 5 V rail from the internal 1.1 V bandgap (+-10 %, so a sanity check only)
  uint16_t s = adcSum(0x0E, 8);
  return s ? (uint16_t)(1100UL * 1023UL * 8UL / s) : 0;
}

// ================================================================ TMC2209 UART
// Single wire: TX drives PDN_UART through R16 (1k), RX reads it directly, so
// every byte sent comes straight back (the echo). Echo good + no reply means
// the MCU side is fine and the problem is at the driver.
// 117,647 baud (UBRR 16, U2X). The driver measures the baud rate from each
// request's sync byte and answers at that rate, so the +2 % offset from
// 115,200 does not matter.
void uartInit() {
  PORTD |= _BV(PD0);                                   // pull-up: an open RX idles high
  UBRR0 = 16;
  UCSR0A = _BV(U2X0);
  UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
  UCSR0B = _BV(RXEN0) | _BV(TXEN0);
}

int16_t uartRx(uint16_t us) {                          // byte, -1 timeout, -2 framing error
  uint16_t t0 = (uint16_t)micros();
  for (;;) {
    uint8_t st = UCSR0A;
    if (st & _BV(RXC0)) {
      uint8_t b = UDR0;
      return (st & _BV(FE0)) ? -2 : b;
    }
    if ((uint16_t)((uint16_t)micros() - t0) > us) return -1;
  }
}

uint8_t tmcCrc(const uint8_t* d, uint8_t n) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < n; i++) {
    uint8_t b = d[i];
    for (uint8_t j = 0; j < 8; j++) {
      if ((crc >> 7) ^ (b & 1)) crc = (uint8_t)(crc << 1) ^ 0x07;
      else crc <<= 1;
      b >>= 1;
    }
  }
  return crc;
}

uint8_t tmcXfer(const uint8_t* tx, uint8_t n, uint8_t* rx, uint8_t nrx) {
  while (UCSR0A & _BV(RXC0)) (void)UDR0;              // flush
  uint8_t good = 0, bad = 0, fe = 0;
  for (uint8_t i = 0; i < n; i++) {
    while (!(UCSR0A & _BV(UDRE0))) { }
    UDR0 = tx[i];
    int16_t e = uartRx(400);                           // a byte takes 85 us
    if (e == -2) fe++;
    else if (e >= 0) { if ((uint8_t)e == tx[i]) good++; else bad++; }
  }
  echoState = (good == n) ? 0 : (fe ? 3 : ((good + bad) == 0 ? 1 : 2));
  for (uint8_t i = 0; i < nrx; i++) {
    int16_t b = uartRx(i == 0 ? 4000 : 500);
    if (b < 0) return i == 0 ? TE_NOREPLY : TE_PARTIAL;
    rx[i] = (uint8_t)b;
  }
  return TE_OK;
}

uint8_t tmcWriteA(uint8_t addr, uint8_t reg, uint32_t v) {
  uint8_t d[8] = {0x05, addr, (uint8_t)(reg | 0x80), (uint8_t)(v >> 24), (uint8_t)(v >> 16),
                  (uint8_t)(v >> 8), (uint8_t)v, 0};
  d[7] = tmcCrc(d, 7);
  tmcXfer(d, 8, 0, 0);
  _delay_us(100);                                      // idle gap between datagrams
  return echoState;                                    // 0 = sent cleanly
}

uint8_t tmcReadA(uint8_t addr, uint8_t reg, uint32_t* v) {
  uint8_t d[4] = {0x05, addr, reg, 0};
  d[3] = tmcCrc(d, 3);
  uint8_t r[8];
  uint8_t e = tmcXfer(d, 4, r, 8);
  _delay_us(100);
  if (e) return e;
  if (r[7] != tmcCrc(r, 7)) return TE_CRC;
  if (r[0] != 0x05 || r[1] != 0xFF || r[2] != reg) return TE_HDR;
  *v = ((uint32_t)r[3] << 24) | ((uint32_t)r[4] << 16) | ((uint16_t)r[5] << 8) | r[6];
  return TE_OK;
}

uint8_t tmcRead(uint8_t reg, uint32_t* v) {
  uint8_t e = 0;
  for (uint8_t i = 0; i < 3; i++) {
    e = tmcReadA(tmcAddr, reg, v);
    if (e == TE_OK) return TE_OK;
  }
  return e;
}

uint8_t tmcWrite(uint8_t reg, uint32_t v) { return tmcWriteA(tmcAddr, reg, v); }

// ---------------------------------------------------------------- driver config
// Current: I_rms = (CS+1)/32 * Vfs/(Rsense + 0.02) / sqrt(2), Rsense 0.11 (BTT V1.2)
//   vsense=1 (Vfs 0.180 V): CS+1 = I[A] * 32.684, 30.6 mA per step, 979 mA max
//   vsense=0 (Vfs 0.325 V): CS+1 = I[A] * 18.102, 55.2 mA per step, 1768 mA max
// vsense=1 is used whenever it reaches, for the finer steps.
void calcCurrent() {
  uint32_t m = S.mA;
  int16_t cs = (int16_t)((m * 32684UL + 500000UL) / 1000000UL) - 1;
  if (cs <= 31) gVs = 1;
  else { gVs = 0; cs = (int16_t)((m * 18102UL + 500000UL) / 1000000UL) - 1; }
  if (cs < 0) cs = 0;
  if (cs > 31) cs = 31;
  gCS = (uint8_t)cs;
}

uint16_t actualmA() { return (uint16_t)((uint32_t)(gCS + 1) * (gVs ? 30596UL : 55243UL) / 1000UL); }

uint32_t gconfVal(uint8_t spread) {
  // pdn_disable (PDN pin is UART), mstep_reg_select (MS pins are the address),
  // multistep_filt, I_scale_analog = 0 (current from registers, module pot ignored)
  return 0x1C0UL | ((spread ^ (uint8_t)spreadPin) ? 0x04UL : 0);
}

uint32_t chopVal(uint8_t mresExp, uint8_t toff) {
  // toff 3, hstrt 5, hend 0, tbl 2 (Klipper's TMC2209 values), intpol on
  return 0x10010050UL | toff | ((uint32_t)gVs << 17) | ((uint32_t)(8 - mresExp) << 24);
}

uint32_t iholdVal(bool fullHold) {
  return (8UL << 16) | ((uint16_t)gCS << 8) | (fullHold ? gCS : gCS / 2);
}

// Writes the full config; true if IFCNT counted every write.
bool applyConfig() {
  uint32_t c0 = 0, c1 = 0;
  bool ok = (tmcRead(R_IFCNT, &c0) == TE_OK);
  tmcWrite(R_GCONF, gconfVal(S.spread));
  tmcWrite(R_IHOLD_IRUN, iholdVal(false));
  tmcWrite(R_TPOWERDOWN, 20);                          // ~0.44 s, then hold current
  tmcWrite(R_TPWMTHRS, 0);                             // StealthChop at every speed
  tmcWrite(R_TCOOLTHRS, 0);                            // no CoolStep, no stall output on DIAG
  tmcWrite(R_SGTHRS, 0);
  tmcWrite(R_COOLCONF, 0);
  tmcWrite(R_PWMCONF, PWMCONF_VAL);
  tmcWrite(R_CHOPCONF, chopVal(S.mres, 3));
  tmcWrite(R_VACTUAL, 0);
  vactNow = 0;
  ok &= (tmcRead(R_IFCNT, &c1) == TE_OK);
  return ok && (uint8_t)(c1 - c0) == 10;
}

// ================================================================ speed math
uint16_t spr() { return S.spr400 ? 400 : 200; }

// |speed| (0.01 rpm) to microsteps per minute at the given resolution
uint32_t ustepsPerMin(int32_t c, uint8_t mresExp) {
  uint32_t a = (uint32_t)(c < 0 ? -c : c);
  return a * spr() / 100UL * (1UL << mresExp);         // max 200000*4*256 = 2.05e8
}

int32_t vactFor(int32_t c) {
  // VACTUAL = usteps/s * 2^24 / 12 MHz = usteps/min * 0.0233017; 179/7680 is 0.024 % high
  uint32_t p = ustepsPerMin(c, S.mres);
  uint32_t v = (p < 23000000UL) ? p * 179UL / 7680UL : (p / 60UL) * 179UL / 128UL;
  if (v > 0x7FFFFFUL) v = 0x7FFFFFUL;
  return c < 0 ? -(int32_t)v : (int32_t)v;
}

int32_t maxCrpm() {
  if (!S.stepDrv) return MAX_CRPM;
  // STEP pin capped at 100 kHz: rpm = 100000*60 / (spr * microsteps)
  int32_t m = (int32_t)(600000000UL / ((uint32_t)spr() * (1UL << S.mres)));
  return m < MAX_CRPM ? m : MAX_CRPM;
}

// ================================================================ STEP pulses (Timer1 -> OC1A = PB1)
// Fast PWM mode 14 (TOP = ICR1), ~4 us high pulses. ICR1 is not double
// buffered, so speed changes are applied in the overflow interrupt, right
// after the counter wraps.
ISR(TIMER1_OVF_vect) {
  if (t1Pend) {
    TCCR1B = (uint8_t)((TCCR1B & 0xF8) | t1Cs);
    ICR1 = t1Top;
    OCR1A = t1Ocr;
    t1Pend = 0;
  }
  TIMSK1 = 0;
}

void stepStop() {
  TIMSK1 = 0;
  TCCR1B = 0;
  TCCR1A = 0;                                          // PB1 back to PORTB, which is low
  PORTB &= ~_BV(PB1);
  stepOn = 0;
}

void stepSet(int32_t c) {
  static const uint16_t PRE[5] PROGMEM = {1, 8, 64, 256, 1024};
  uint32_t p = ustepsPerMin(c, S.mres);                // = STEP pulses per minute
  if (p < 15) { stepStop(); return; }                  // under 0.25 Hz: stopped
  if (p > 6000000UL) p = 6000000UL;                    // 100 kHz cap
  bool neg = c < 0;
  if (stepOn && neg != stepNeg) stepStop();
  if (!stepOn) {
    if (neg) DIR_HI(); else DIR_LO();
    stepNeg = neg;
    _delay_us(5);                                      // DIR setup time, generous for J5 drivers
  }
  uint8_t cs;
  uint32_t ticks = 0;
  for (cs = 1; cs <= 5; cs++) {                        // ticks = 16 MHz * 60 / (N * p)
    ticks = 960000000UL / pgm_read_word(&PRE[cs - 1]) / p;
    if (ticks <= 65536UL) break;
  }
  if (cs > 5) { stepStop(); return; }
  uint16_t n = pgm_read_word(&PRE[cs - 1]);
  uint16_t ocr = (uint16_t)((64 + n - 1) / n) - 1;     // 64 cycles = 4 us
  uint16_t topv = (uint16_t)(ticks - 1);
  if (!stepOn) {
    TCCR1B = 0;
    TCCR1A = _BV(COM1A1) | _BV(WGM11);
    ICR1 = topv;
    OCR1A = ocr;
    TCNT1 = 0;
    TIFR1 = 0xFF;
    TCCR1B = _BV(WGM13) | _BV(WGM12) | cs;
    stepOn = 1;
  } else {
    cli();
    if (cs == (TCCR1B & 7) && TCNT1 + (cs == 1 ? 64 : 4) < topv) {
      ICR1 = topv;                                     // counter still short of the new TOP: safe to apply now
      OCR1A = ocr;
      t1Pend = 0;
      TIMSK1 = 0;
    } else {                                           // otherwise apply right after the next wrap
      t1Top = topv; t1Ocr = ocr; t1Cs = cs; t1Pend = 1;
      TIFR1 = _BV(TOV1);
      TIMSK1 = _BV(TOIE1);
    }
    sei();
  }
}

// ================================================================ motion
void outSpeed(int32_t c) {
  if (S.stepDrv) { stepSet(c); return; }
  int32_t v = vactFor(c);
  if (v != vactNow && tmcWrite(R_VACTUAL, (uint32_t)v & 0xFFFFFFUL) == 0) vactNow = v;
}

void motorKill() {
  if (tmcOk) tmcWrite(R_VACTUAL, 0);
  vactNow = 0;
  stepStop();
  DIR_LO();
  EN_OFF();
  spNow = 0;
  mState = M_OFF;
}

void toast(PGM_P m) { toastMsg = m; toastUntil = millis() + 1500; tDraw = 0; }

void setFault(uint8_t f) {
  motorKill();
  fault = f;
  DBG("FAULT %u  DRV_STATUS=%08lX VM=%umV\n", f, drvStatus, vmMv);
}

void motorStart() {
  if (fault) return;
  if (!tmcOk && !S.stepDrv) { toast(PSTR("No driver link")); return; }
  if (tmcOk && vmMv < VM_RUN_MV) { toast(PSTR("Needs 12V on VM")); return; }
  if (tmcOk) {
    tmcWrite(R_GSTAT, 7);                              // clear old flags
    tmcWrite(R_VACTUAL, 0);
  } else toast(PSTR("No TMC: J5 pulses only"));      // STEP mode can still drive an external driver
  vactNow = 0;
  spNow = 0;
  sgLowCount = 0;
  EN_ON();
  mState = M_START;                                    // 150 ms still: StealthChop tunes at standstill
  mT = millis();
}

void motorStop() {
  if (mState == M_START) motorKill();
  else if (mState == M_RUN) mState = M_STOP;
}

void motorTask(uint32_t now) {
  if (mState == M_OFF) return;
  if (mState == M_START) {
    if (now - mT >= 150) { mState = M_RUN; rampT = now; steadyT = now; }
    return;
  }
  uint32_t dt = now - rampT;
  if (dt < 5) return;
  if (dt > 100) dt = 100;
  rampT = now;
  int32_t tgt = 0;
  if (mState == M_RUN) {
    int32_t m = maxCrpm();
    tgt = S.speed > m ? m : (S.speed < -m ? -m : S.speed);
  }
  uint16_t acc = pgm_read_word(&ACCEL_TAB[S.accel]);
  int32_t before = spNow;
  if (acc == 0) spNow = tgt;
  else {
    int32_t stepc = (int32_t)acc * (int32_t)dt / 10;  // rpm/s -> 0.01 rpm per dt ms
    if (spNow < tgt) spNow = (tgt - spNow > stepc) ? spNow + stepc : tgt;
    else if (spNow > tgt) spNow = (spNow - tgt > stepc) ? spNow - stepc : tgt;
  }
  if (spNow != before || (S.stepDrv && !stepOn && spNow)) outSpeed(spNow);
  if (spNow != tgt || spNow != before) steadyT = now;
  if (mState == M_STOP && spNow == 0) { motorKill(); saveSettings(); }
}

// ================================================================ I2C master (polling, every wait bounded)
bool twiWait() {
  uint16_t n = 0;
  while (!(TWCR & _BV(TWINT))) if (++n == 0) return false;   // ~16 ms worst case
  return true;
}

void twiInit() {
  TWCR = 0;
  TWSR = 0;
  TWBR = oledFast ? 12 : 72;                           // 16 MHz/(16 + 2*TWBR): 400 kHz or 100 kHz
  TWCR = _BV(TWEN);
}

void twiStop() {
  TWCR = _BV(TWINT) | _BV(TWSTO) | _BV(TWEN);
  uint8_t n = 0;
  while ((TWCR & _BV(TWSTO)) && ++n) { }
}

bool twiStart(uint8_t a) {
  TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
  if (!twiWait()) return false;
  uint8_t st = TWSR & 0xF8;
  if (st != 0x08 && st != 0x10) return false;
  TWDR = (uint8_t)(a << 1);
  TWCR = _BV(TWINT) | _BV(TWEN);
  return twiWait() && (TWSR & 0xF8) == 0x18;           // address ACKed
}

bool twiByte(uint8_t b) {
  TWDR = b;
  TWCR = _BV(TWINT) | _BV(TWEN);
  return twiWait() && (TWSR & 0xF8) == 0x28;           // data ACKed
}

bool i2cProbe(uint8_t a) {
  bool ok = twiStart(a);
  twiStop();
  if (!ok) twiInit();
  return ok;
}

// ================================================================ OLED (SSD1306, 128x64)
// Classic 5x7 font (Adafruit GFX glcdfont, BSD), ASCII 32..126, LSB = top row.
static const uint8_t FONT[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5F, 0x00, 0x00, 0x00, 0x07, 0x00, 0x07, 0x00, 0x14, 0x7F, 0x14, 0x7F, 0x14,
  0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x23, 0x13, 0x08, 0x64, 0x62, 0x36, 0x49, 0x56, 0x20, 0x50, 0x00, 0x08, 0x07, 0x03, 0x00,
  0x00, 0x1C, 0x22, 0x41, 0x00, 0x00, 0x41, 0x22, 0x1C, 0x00, 0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x08, 0x08, 0x3E, 0x08, 0x08,
  0x00, 0x80, 0x70, 0x30, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x60, 0x60, 0x00, 0x20, 0x10, 0x08, 0x04, 0x02,
  0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00, 0x42, 0x7F, 0x40, 0x00, 0x72, 0x49, 0x49, 0x49, 0x46, 0x21, 0x41, 0x49, 0x4D, 0x33,
  0x18, 0x14, 0x12, 0x7F, 0x10, 0x27, 0x45, 0x45, 0x45, 0x39, 0x3C, 0x4A, 0x49, 0x49, 0x31, 0x41, 0x21, 0x11, 0x09, 0x07,
  0x36, 0x49, 0x49, 0x49, 0x36, 0x46, 0x49, 0x49, 0x29, 0x1E, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x40, 0x34, 0x00, 0x00,
  0x00, 0x08, 0x14, 0x22, 0x41, 0x14, 0x14, 0x14, 0x14, 0x14, 0x00, 0x41, 0x22, 0x14, 0x08, 0x02, 0x01, 0x59, 0x09, 0x06,
  0x3E, 0x41, 0x5D, 0x59, 0x4E, 0x7C, 0x12, 0x11, 0x12, 0x7C, 0x7F, 0x49, 0x49, 0x49, 0x36, 0x3E, 0x41, 0x41, 0x41, 0x22,
  0x7F, 0x41, 0x41, 0x41, 0x3E, 0x7F, 0x49, 0x49, 0x49, 0x41, 0x7F, 0x09, 0x09, 0x09, 0x01, 0x3E, 0x41, 0x41, 0x51, 0x73,
  0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00, 0x41, 0x7F, 0x41, 0x00, 0x20, 0x40, 0x41, 0x3F, 0x01, 0x7F, 0x08, 0x14, 0x22, 0x41,
  0x7F, 0x40, 0x40, 0x40, 0x40, 0x7F, 0x02, 0x1C, 0x02, 0x7F, 0x7F, 0x04, 0x08, 0x10, 0x7F, 0x3E, 0x41, 0x41, 0x41, 0x3E,
  0x7F, 0x09, 0x09, 0x09, 0x06, 0x3E, 0x41, 0x51, 0x21, 0x5E, 0x7F, 0x09, 0x19, 0x29, 0x46, 0x26, 0x49, 0x49, 0x49, 0x32,
  0x03, 0x01, 0x7F, 0x01, 0x03, 0x3F, 0x40, 0x40, 0x40, 0x3F, 0x1F, 0x20, 0x40, 0x20, 0x1F, 0x3F, 0x40, 0x38, 0x40, 0x3F,
  0x63, 0x14, 0x08, 0x14, 0x63, 0x03, 0x04, 0x78, 0x04, 0x03, 0x61, 0x59, 0x49, 0x4D, 0x43, 0x00, 0x7F, 0x41, 0x41, 0x41,
  0x02, 0x04, 0x08, 0x10, 0x20, 0x00, 0x41, 0x41, 0x41, 0x7F, 0x04, 0x02, 0x01, 0x02, 0x04, 0x40, 0x40, 0x40, 0x40, 0x40,
  0x00, 0x03, 0x07, 0x08, 0x00, 0x20, 0x54, 0x54, 0x78, 0x40, 0x7F, 0x28, 0x44, 0x44, 0x38, 0x38, 0x44, 0x44, 0x44, 0x28,
  0x38, 0x44, 0x44, 0x28, 0x7F, 0x38, 0x54, 0x54, 0x54, 0x18, 0x00, 0x08, 0x7E, 0x09, 0x02, 0x18, 0xA4, 0xA4, 0x9C, 0x78,
  0x7F, 0x08, 0x04, 0x04, 0x78, 0x00, 0x44, 0x7D, 0x40, 0x00, 0x20, 0x40, 0x40, 0x3D, 0x00, 0x7F, 0x10, 0x28, 0x44, 0x00,
  0x00, 0x41, 0x7F, 0x40, 0x00, 0x7C, 0x04, 0x78, 0x04, 0x78, 0x7C, 0x08, 0x04, 0x04, 0x78, 0x38, 0x44, 0x44, 0x44, 0x38,
  0xFC, 0x18, 0x24, 0x24, 0x18, 0x18, 0x24, 0x24, 0x18, 0xFC, 0x7C, 0x08, 0x04, 0x04, 0x08, 0x48, 0x54, 0x54, 0x54, 0x24,
  0x04, 0x04, 0x3F, 0x44, 0x24, 0x3C, 0x40, 0x40, 0x20, 0x7C, 0x1C, 0x20, 0x40, 0x20, 0x1C, 0x3C, 0x40, 0x30, 0x40, 0x3C,
  0x44, 0x28, 0x10, 0x28, 0x44, 0x4C, 0x90, 0x90, 0x90, 0x7C, 0x44, 0x64, 0x54, 0x4C, 0x44, 0x00, 0x08, 0x36, 0x41, 0x00,
  0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x41, 0x36, 0x08, 0x00, 0x02, 0x01, 0x02, 0x04, 0x02};
// nibble doubler for the 2x font: bit k -> bits 2k and 2k+1
static const uint8_t EXP2[16] PROGMEM = {0x00, 0x03, 0x0C, 0x0F, 0x30, 0x33, 0x3C, 0x3F,
                                         0xC0, 0xC3, 0xCC, 0xCF, 0xF0, 0xF3, 0xFC, 0xFF};
// Same init sequence as U8g2's SSD1306 128x64 "noname" driver (proven on this OLED)
static const uint8_t OLED_INIT[] PROGMEM = {0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14,
                                            0x20, 0x00, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40,
                                            0x2E, 0xA4, 0xA6};

// bar cells: 0x10+n = n of 6 columns filled (n = 0..6), 0x17 = right end cap
#define BAR(n) ((char)(0x10 + (n)))

uint8_t glyph(uint8_t u, uint8_t i) {
  if (u >= 0x10 && u <= 0x16) return i < (u - 0x10) ? 0x7E : 0x42;
  if (u == 0x17) return i == 0 ? 0x7E : 0;
  if (i == 5 || u < 32 || u > 126) return 0;
  return pgm_read_byte(&FONT[(u - 32) * 5 + i]);
}

bool oledCmd(const uint8_t* p, uint8_t n, bool pgm) {
  bool ok = twiStart(oledAddr) && twiByte(0x00);
  for (uint8_t i = 0; i < n && ok; i++) ok = twiByte(pgm ? pgm_read_byte(p + i) : p[i]);
  twiStop();
  return ok;
}

void oledFlip() {
  uint8_t c[2] = {(uint8_t)(S.flip ? 0xA0 : 0xA1), (uint8_t)(S.flip ? 0xC0 : 0xC8)};
  oledCmd(c, 2, false);
}

uint8_t rowByte(uint8_t r, uint8_t x) {
  if (bigRow < 8 && (r == bigRow || r == bigRow + 1)) {
    uint8_t len = (uint8_t)strlen(bigS);
    if (x >= bigX && x < bigX + len * 12) {
      uint8_t k = (uint8_t)((x - bigX) / 12), gx = (uint8_t)(((x - bigX) % 12) >> 1);
      uint8_t col = glyph((uint8_t)bigS[k], gx);
      return pgm_read_byte(&EXP2[r == bigRow ? (col & 0x0F) : (col >> 4)]);
    }
  }
  uint8_t c = x / 6;
  uint8_t b = c < 21 ? glyph((uint8_t)tb[r][c], x % 6) : 0;
  if ((tinv[r] >> (c < 21 ? c : 31)) & 1) b ^= 0xFF;
  return b;
}

bool oledRow(uint8_t r) {
  uint8_t w[6] = {0x21, 0, 127, 0x22, r, r};           // column and page window
  if (!oledCmd(w, 6, false)) return false;
  bool ok = twiStart(oledAddr) && twiByte(0x40);
  for (uint8_t x = 0; x < 128 && ok; x++) ok = twiByte(rowByte(r, x));
  twiStop();
  return ok;
}

uint16_t rowHash(uint8_t r) {
  uint16_t h = 0x9E37 + r;
  const uint8_t* iv = (const uint8_t*)&tinv[r];
  for (uint8_t i = 0; i < 4; i++) h = (uint16_t)((h << 3) | (h >> 13)) ^ iv[i];
  for (uint8_t c = 0; c < 21; c++) h = (uint16_t)((h << 3) | (h >> 13)) ^ (uint8_t)tb[r][c];
  if (bigRow < 8 && (r == bigRow || r == bigRow + 1)) {
    h ^= bigX;
    for (uint8_t c = 0; bigS[c]; c++) h = (uint16_t)((h << 5) | (h >> 11)) ^ (uint8_t)bigS[c];
  }
  return h ? h : 1;
}

void oledBegin() {
  twiInit();
  if (!oledCmd(OLED_INIT, sizeof OLED_INIT, true)) { oledOk = false; return; }
  oledFlip();
  oledOk = true;
  memset(rowSum, 0, sizeof rowSum);
  uint8_t on = 0xAF;
  oledCmd(&on, 1, false);
}

// ---------------------------------------------------------------- text buffer
void clr() { memset(tb, ' ', sizeof tb); memset(tinv, 0, sizeof tinv); bigRow = 0xFF; }
void put(uint8_t r, uint8_t c, const char* s) { while (*s && c < 21) tb[r][c++] = *s++; }
void putP(uint8_t r, uint8_t c, PGM_P s) { char ch; while ((ch = (char)pgm_read_byte(s++)) && c < 21) tb[r][c++] = ch; }
void putF(uint8_t r, uint8_t c, PGM_P fmt, ...) {
  char b[24];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf_P(b, sizeof b, fmt, ap);
  va_end(ap);
  put(r, c, b);
}
void putRight(uint8_t r, const char* s) { uint8_t n = (uint8_t)strlen(s); put(r, n < 21 ? 21 - n : 0, s); }
void invRow(uint8_t r) { tinv[r] = 0xFFFFFFFFUL; }
void invSpan(uint8_t r, uint8_t c0, uint8_t n) { while (n-- && c0 < 21) tinv[r] |= 1UL << c0++; }

// Pushes every changed row now (blocking, ~25 ms). Used around blocking work.
void flushScreen() {
  if (!oledOk) return;
  for (uint8_t r = 0; r < 8; r++) {
    uint16_t h = rowHash(r);
    if (h != rowSum[r] && oledRow(r)) rowSum[r] = h;
  }
}

void showBusy(PGM_P msg) {
  clr();
  putP(0, 0, PSTR("STEPPER TESTER " FW_VER));
  invRow(0);
  putP(3, 1, msg);
  flushScreen();
  tDraw = millis();
}

// ================================================================ self test
#define CLK_MHZ10(t) ((uint16_t)((uint32_t)(t) * 16UL / 25UL))   // 250 ticks = 16.0 MHz

void wdtSet(uint8_t v) {                               // timed WDTCSR write
  uint8_t s = SREG;
  cli();
  wdt_reset();
  __asm__ __volatile__("sts %0, %1\n\t"
                       "sts %0, %2\n\t"
                       : : "n"(_SFR_MEM_ADDR(WDTCSR)), "r"((uint8_t)(_BV(WDCE) | _BV(WDE))), "r"(v)
                       : "memory");
  SREG = s;
}

// CPU clock vs the watchdog's own ~128 kHz RC oscillator: Timer1 at clk/1024
// is read in the watchdog interrupt, one 16 ms period after a reset of the
// watchdog. 16 MHz -> ~250 ticks, 8 MHz -> ~125, 1 MHz -> ~16. The watchdog
// goes back to its 2 s reset mode afterwards.
static volatile uint16_t wdtTicks;
static volatile uint8_t  wdtFired;
ISR(WDT_vect) { wdtTicks = TCNT1; wdtFired = 1; }

uint16_t measureClock() {
  cli();
  wdtFired = 0;
  wdtSet(_BV(WDIE));                                   // interrupt mode, 16 ms
  TCCR1A = 0; TCCR1B = 0; TCNT1 = 0; TIFR1 = 0xFF;
  TCCR1B = _BV(CS12) | _BV(CS10);
  wdt_reset();
  sei();
  for (uint16_t i = 0; i < 2000 && !wdtFired; i++) _delay_us(50);   // give up after ~100 ms
  cli();
  TCCR1B = 0;
  wdtSet(0);
  wdt_enable(WDTO_2S);
  sei();
  return wdtFired ? wdtTicks : 0;
}

void setRes(uint8_t t, uint8_t r, uint8_t code, int16_t v) { tRes[t] = r; tCode[t] = code; tVal[t] = v; }

void testOled() {
  // Both lines must idle high (R17/R18 4.7k + module pull-ups + internal).
  DDRC &= ~(_BV(PC4) | _BV(PC5));
  PORTC |= _BV(PC4) | _BV(PC5);
  _delay_us(50);
  if ((PINC & _BV(PC5)) && !(PINC & _BV(PC4))) {       // SDA held by a slave: clock it free
    for (uint8_t i = 0; i < 9; i++) {
      DDRC |= _BV(PC5); _delay_us(10);
      DDRC &= ~_BV(PC5); _delay_us(10);
    }
  }
  uint8_t lines = PINC & (_BV(PC4) | _BV(PC5));
  if (lines != (_BV(PC4) | _BV(PC5))) {
    setRes(T_OLED, R_FAIL, 1, (lines & _BV(PC4) ? 0 : 1) | (lines & _BV(PC5) ? 0 : 2));
    return;
  }
  for (oledFast = 1; ; oledFast = 0) {                 // 400 kHz first, then 100 kHz
    twiInit();
    oledAddr = i2cProbe(0x3C) ? 0x3C : (i2cProbe(0x3D) ? 0x3D : 0);
    if (oledAddr || !oledFast) break;
  }
  if (!oledAddr) { setRes(T_OLED, R_FAIL, 2, 0); return; }
  oledBegin();
  if (!oledOk) { setRes(T_OLED, R_FAIL, 2, oledAddr); return; }
  setRes(T_OLED, oledFast ? R_OK : R_WARN, oledFast ? 0 : 3, oledAddr);
}

// Driver tests: echo, link (with address scan), identity + writes, pin
// readback, DIAG wiring, output short check.
void testDriver() {
  uint32_t v = 0;
  tmcOk = false;
  tmcAddr = 0;
  uint8_t e = tmcReadA(0, R_IFCNT, &v);
  setRes(T_UART, echoState ? R_FAIL : R_OK, echoState, 0);   // MCU side, judged from the echo
  bool vmOk = vmMv >= VM_OK_MV;

  if (echoState) setRes(T_LINK, R_SKIP, 0, 0);
  else if (!vmOk && e != TE_OK) setRes(T_LINK, R_SKIP, 1, 0);     // the driver needs VM to talk
  else {
    for (uint8_t i = 0; i < 3 && e != TE_OK; i++) { delay(5); e = tmcReadA(0, R_IFCNT, &v); }
    if (e == TE_OK) setRes(T_LINK, R_OK, 0, 0);
    else {
      for (uint8_t a = 1; a < 4 && !tmcAddr; a++)
        if (tmcReadA(a, R_IFCNT, &v) == TE_OK) tmcAddr = a;
      if (tmcAddr) setRes(T_LINK, R_FAIL, 2, tmcAddr);        // talks, but on the wrong address
      else setRes(T_LINK, R_FAIL, (e == TE_CRC || e == TE_HDR) ? 3 : 1, e);
    }
  }
  if (!(tRes[T_LINK] == R_OK || tmcAddr)) {
    setRes(T_DRV, R_SKIP, 0, 0); setRes(T_PINS, R_SKIP, 0, 0);
    setRes(T_DIAG, R_SKIP, 0, 0); setRes(T_OUT, R_SKIP, 0, 0);
    return;
  }

  // T_DRV: version, SPREAD pin, every write counted, config reads back
  uint32_t io = 0, gs = 0, g = 0, ch = 0, pw = 0;
  bool ioOk = tmcRead(R_IOIN, &io) == TE_OK;
  uint8_t ver = (uint8_t)(io >> 24);
  spreadPin = (io >> 8) & 1;
  tmcWrite(R_GSTAT, 7);                                // clear the power-up reset flag
  bool cfg = applyConfig();
  bool rb = tmcRead(R_GCONF, &g) == TE_OK && tmcRead(R_CHOPCONF, &ch) == TE_OK && tmcRead(R_PWMCONF, &pw) == TE_OK;
  rb = rb && (g & 0x3FF) == gconfVal(S.spread) && ch == chopVal(S.mres, 3) && pw == PWMCONF_VAL;
  tmcRead(R_GSTAT, &gs);
  if (!ioOk) setRes(T_DRV, R_FAIL, 5, 0);
  else if (ver != 0x21) setRes(T_DRV, R_FAIL, 1, ver);
  else if (!cfg) setRes(T_DRV, R_FAIL, 2, 0);
  else if (!rb) setRes(T_DRV, R_FAIL, 4, 0);
  else if (gs & 0x04) setRes(T_DRV, R_WARN, 3, (int16_t)gs);
  else setRes(T_DRV, R_OK, 0, ver);
  DBG("  IOIN=%08lX GSTAT=%02X GCONF=%03lX CHOPCONF=%08lX PWMCONF=%08lX\n", io, (uint8_t)gs, g, ch, pw);

  // T_PINS: drive EN, DIR, STEP and read them back through IOIN.
  // toff = 0 first, so EN low cannot energize anything.
  tmcWrite(R_CHOPCONF, chopVal(S.mres, 0));
  uint32_t a = 0, b = 0;
  EN_OFF(); DIR_LO(); PORTB &= ~_BV(PB1); delay(1);
  bool r1 = tmcRead(R_IOIN, &a) == TE_OK;
  EN_ON(); DIR_HI(); PORTB |= _BV(PB1); delay(1);
  bool r2 = tmcRead(R_IOIN, &b) == TE_OK;
  EN_OFF(); DIR_LO(); PORTB &= ~_BV(PB1);
  if (!r1 || !r2) setRes(T_PINS, R_FAIL, 8, 0);
  else {
    uint8_t bad = 0;
    if (!(a & 0x001) || (b & 0x001)) bad |= 1;         // ENN  bit 0
    if ((a & 0x200) || !(b & 0x200)) bad |= 2;         // DIR  bit 9
    if ((a & 0x080) || !(b & 0x080)) bad |= 4;         // STEP bit 7
    setRes(T_PINS, bad ? R_FAIL : R_OK, bad, bad);
  }
  tmcWrite(R_CHOPCONF, chopVal(S.mres, 3));

  // T_DIAG: with EN high the driver holds DIAG low. PB0 has its pull-up on,
  // so reading high means nothing drives it (module pin not wired to J7).
  delay(2);
  bool pinHi = DIAG_HI();
  diagWired = !pinHi;
  if (!pinHi) setRes(T_DIAG, R_OK, 0, 0);
  else setRes(T_DIAG, R_WARN, ((a >> 4) & 1) ? 2 : 1, 0);

  // T_OUT: energize at ~306 mA in SpreadCycle for 80 ms, then read the
  // driver's short-to-GND / short-to-supply latches. The coil LEDs flash.
  if (!vmOk) setRes(T_OUT, R_SKIP, 1, 0);
  else {
    uint8_t keepCS = gCS, keepVs = gVs;
    gCS = 9; gVs = 1;                                  // (9+1) * 30.6 mA
    tmcWrite(R_GCONF, gconfVal(1));
    tmcWrite(R_IHOLD_IRUN, iholdVal(true));
    tmcWrite(R_CHOPCONF, chopVal(S.mres, 3));
    tmcWrite(R_GSTAT, 7);
    EN_ON();
    delay(80);
    uint32_t ds = 0, g2 = 0;
    bool rr = tmcRead(R_DRV_STATUS, &ds) == TE_OK;
    tmcRead(R_GSTAT, &g2);
    EN_OFF();
    gCS = keepCS; gVs = keepVs;
    if (!rr) setRes(T_OUT, R_FAIL, 4, 0);
    else if (ds & DS_SHORT) setRes(T_OUT, R_FAIL, 1, (int16_t)((ds >> 2) & 0x0F));
    else if (ds & DS_OT) setRes(T_OUT, R_FAIL, 2, 0);
    else if (g2 & 0x02) setRes(T_OUT, R_FAIL, 3, 0);
    else setRes(T_OUT, R_OK, 0, 0);
    DBG("  output check: DRV_STATUS=%08lX GSTAT=%02X\n", ds, (uint8_t)g2);
  }
  applyConfig();
  tmcWrite(R_GSTAT, 7);
  tmcOk = true;
}

void printPost();

void runPost() {
  motorKill();
  fault = F_NONE;
  LED_ON();
  memset(tRes, 0, sizeof tRes);

  // 1. CPU clock and fuses
  clkTicks = measureClock();
  uint16_t m10 = CLK_MHZ10(clkTicks);
  cli();
  lfuse = boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS);
  hfuse = boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS);
  efuse = boot_lock_fuse_bits_get(GET_EXTENDED_FUSE_BITS);
  sei();
  // WDT RC is 128 kHz nominal but ~110-120 kHz at 5 V, so 16 MHz reads ~16-19.
  // 12.8 separates 16 MHz from 8 MHz for any WDT clock between 90 and 140 kHz.
  if (m10 >= 128) setRes(T_CLK, R_OK, 0, m10);
  else setRes(T_CLK, R_FAIL, m10 >= 50 ? 1 : 2, m10);
  wdt_reset();

  // 2. 5 V rail
  vccMv = readVcc();
  if (vccMv >= 4400 && vccMv <= 5600) setRes(T_5V, R_OK, 0, (int16_t)vccMv);    // bandgap is 1.0-1.2 V
  else setRes(T_5V, R_WARN, vccMv < 4400 ? 1 : 2, (int16_t)vccMv);

  // 3. Motor supply
  vmMv = readVM();
  vmWasAbsent = vmMv < VM_ABSENT_MV;
  int16_t v10 = (int16_t)(vmMv / 10);
  if (vmMv >= VM_MAX_MV) setRes(T_VM, R_FAIL, 4, v10);
  else if (vmMv < VM_ABSENT_MV) setRes(T_VM, R_WARN, 1, v10);
  else if (vmMv < VM_OK_MV) setRes(T_VM, R_WARN, 2, v10);
  else if (vmMv > VM_HIGH_MV) setRes(T_VM, R_WARN, 3, v10);
  else setRes(T_VM, R_OK, 0, v10);

  // 4. OLED: I2C lines, ACK, init
  if (!oledOk) testOled();
  else setRes(T_OLED, oledFast ? R_OK : R_WARN, oledFast ? 0 : 3, oledAddr);
  showBusy(PSTR("Self test..."));
  wdt_reset();

  // 5. Knob button must be released by now
  setRes(T_KNOB, SW_DOWN() ? R_FAIL : R_OK, SW_DOWN() ? 1 : 0, (PIND >> PD2) & 7);

  // 6..11. Driver
  testDriver();
  wdt_reset();

  postFirstFail = 0;
  cur = 0xFF;
  for (uint8_t t = 0; t < T_N; t++) {
    if (tRes[t] == R_FAIL && !postFirstFail) postFirstFail = t + 1;
    if ((tRes[t] == R_FAIL || tRes[t] == R_WARN) && cur == 0xFF) cur = t;
  }
  if (cur == 0xFF) cur = 0;
  top = cur > 4 ? cur - 4 : 0;
  screen = SC_POST;
  tDraw = 0;
  LED_OFF();
  printPost();
}

// ---------------------------------------------------------------- self test text
static const char tn0[] PROGMEM = "CLK";
static const char tn1[] PROGMEM = "5V";
static const char tn2[] PROGMEM = "VM";
static const char tn3[] PROGMEM = "OLED";
static const char tn4[] PROGMEM = "KNOB";
static const char tn5[] PROGMEM = "UART";
static const char tn6[] PROGMEM = "LINK";
static const char tn7[] PROGMEM = "DRV";
static const char tn8[] PROGMEM = "PINS";
static const char tn9[] PROGMEM = "DIAG";
static const char tn10[] PROGMEM = "OUT";
static PGM_P const TNAME[T_N] PROGMEM = {tn0, tn1, tn2, tn3, tn4, tn5, tn6, tn7, tn8, tn9, tn10};

// Detail on the list row, at most 10 characters.
void testDetail(uint8_t t, char* b) {
  int16_t v = tVal[t];
  uint8_t c = tCode[t];
  PGM_P s = 0;
  b[0] = 0;
  if (tRes[t] == R_SKIP) { strcpy_P(b, PSTR("skipped")); return; }
  switch (t) {
    case T_CLK:  s = c == 0 ? PSTR("16 MHz") : c == 1 ? PSTR("~8 MHz") : PSTR("~1-2 MHz"); break;
    case T_5V:   sprintf_P(b, PSTR("%u.%02uV"), v / 1000, (v % 1000) / 10); break;
    case T_VM:   sprintf_P(b, PSTR("%u.%02uV"), v / 100, v % 100); break;
    case T_OLED: if (c == 1) s = PSTR("bus low"); else if (c == 2 && !v) s = PSTR("no ACK"); else sprintf_P(b, PSTR("0x%02X"), v); break;
    case T_KNOB: s = c ? PSTR("stuck") : PSTR("released"); break;
    case T_UART: s = c == 0 ? PSTR("echo ok") : c == 1 ? PSTR("no echo") : c == 2 ? PSTR("garbled") : PSTR("line low"); break;
    case T_LINK: if (c == 2) sprintf_P(b, PSTR("addr %d"), v); else s = c == 0 ? PSTR("addr 0") : c == 3 ? PSTR("bad CRC") : PSTR("no reply"); break;
    case T_DRV:  if (c == 1) sprintf_P(b, PSTR("ver 0x%02X"), v);
                 else s = (c == 0 || c == 3) ? PSTR("TMC2209") : c == 2 ? PSTR("no writes") : PSTR("readback"); break;
    case T_PINS: if (!c) s = PSTR("EN DIR STP");
                 else if (c == 8) s = PSTR("no read");
                 else sprintf_P(b, PSTR("%s%s%s"), (c & 1) ? "EN " : "", (c & 2) ? "DIR " : "", (c & 4) ? "STP" : ""); break;
    case T_DIAG: s = c == 0 ? PSTR("wired") : c == 1 ? PSTR("not wired") : PSTR("high"); break;
    case T_OUT:  s = c == 0 ? PSTR("no short") : c == 1 ? PSTR("SHORT") : c == 2 ? PSTR("hot") : PSTR("error"); break;
  }
  if (s) strcpy_P(b, s);
}

// Two-line hint for the selected test (21 characters per line).
PGM_P testHint(uint8_t t) {
  uint8_t c = tCode[t], r = tRes[t];
  if (r == R_SKIP) {
    if (t == T_LINK && c == 1) return PSTR("Driver only talks\nwith 12V on VM");
    if (t == T_OUT && c == 1) return PSTR("Needs 9V+ on VM");
    return PSTR("Needs the driver\nlink first");
  }
  if (r == R_OK) {
    switch (t) {
      case T_CLK:  return PSTR("16 MHz crystal and\nfuses look right");
      case T_5V:   return PSTR("Bandgap estimate,\n+-10%. Meter TP2");
      case T_VM:   return PSTR("Trim in menu to\nmatch your meter");
      case T_OLED: return PSTR("Display answering\nat 400 kHz");
      case T_KNOB: return PSTR("Button released.\nTurn it: try menus");
      case T_UART: return PSTR("TX, R16 and RX path\nare good");
      case T_LINK: return PSTR("Driver answers\nat address 0");
      case T_DRV:  return PSTR("Version 0x21, writes\ncounted, readback ok");
      case T_PINS: return PSTR("Driver sees EN, DIR\nand STEP toggle");
      case T_DIAG: return PSTR("DIAG reaches PB0");
      case T_OUT:  return PSTR("Outputs on, no short\n(coil LEDs flashed)");
    }
  }
  switch (t) {
    case T_CLK:  return c == 1 ? PSTR("Running ~8MHz RC.\nBurn Bootloader") : PSTR("Clock/8 fuse set.\nBurn Bootloader");
    case T_5V:   return PSTR("5V rail off. Meter\nTP2, check U1 7805");
    case T_VM:   if (c == 1) return PSTR("No 12V on VM. Brick,\nfuse, switch, Q1");
                 if (c == 2) return PSTR("VM low. Brick, fuse,\nswitch, Q1 drop");
                 if (c == 3) return PSTR("Over 16V: D1 is a\n15V zener! Unplug");
                 return PSTR("VM over 25V cap\nrating. UNPLUG NOW");
    case T_OLED: if (c == 1) return PSTR("SDA/SCL held low.\nRibbon flipped/short");
                 if (c == 3) return PSTR("Only works at 100k.\nRibbon/pull-ups weak");
                 return PSTR("No ACK: check ribbon\nJ10/J11, U6 solder");
    case T_KNOB: return PSTR("Button stuck low.\nSW4 / ENC_SW short");
    case T_UART: if (c == 1) return PSTR("No echo. Check R16,\nPD0/PD1 traces");
                 if (c == 2) return PSTR("Echo garbled. Line\ncontention on PDN");
                 return PSTR("PDN line held low.\nShort on UART net");
    case T_LINK: if (c == 2) return PSTR("Driver on wrong addr\nMS1/MS2 not to GND");
                 if (c == 3) return PSTR("Reply CRC bad.\nNoise on PDN line");
                 return PSTR("No reply. Module in\nright way? PDN 4+5");
    case T_DRV:  if (c == 1) return PSTR("Not a TMC2209\n(TMC2208 is 0x20)");
                 if (c == 2) return PSTR("Writes not counted\n(IFCNT). Noise?");
                 if (c == 3) return PSTR("Charge pump low.\nVM dipped?");
                 if (c == 5) return PSTR("IOIN read failed");
                 return PSTR("Config read back\nwrong. Retest");
    case T_PINS: return PSTR("Driver misses a pin.\nCheck that trace");
    case T_DIAG: return c == 1 ? PSTR("DIAG not on J7.\nOptional: jumper it") : PSTR("DIAG high: driver\nflags an error");
    case T_OUT:  if (c == 1) return PSTR("SHORT on outputs!\nWire or J8/12/13");
                 if (c == 2) return PSTR("Driver overtemp.\nHeatsink, current");
                 return PSTR("Driver error flag.\nCheck VM, outputs");
  }
  return PSTR("");
}

static const char rs0[] PROGMEM = "--";
static const char rs1[] PROGMEM = "ok";
static const char rs2[] PROGMEM = "!?";
static const char rs3[] PROGMEM = "XX";
static const char rs4[] PROGMEM = "--";
static PGM_P const RSTAT[5] PROGMEM = {rs0, rs1, rs2, rs3, rs4};

void printPost() {
  char d[12], h[44], nm[6], rs[4];
  DBG("\n=== Stepper Tester fw " FW_VER " self test ===\n");
  DBG("reset cause %02X  fuses L=%02X H=%02X E=%02X  clk ticks %u\n", resetCause, lfuse, hfuse, efuse, clkTicks);
  uint8_t nf = 0, nw = 0;
  for (uint8_t t = 0; t < T_N; t++) {
    testDetail(t, d);
    strncpy_P(h, testHint(t), sizeof h);
    h[sizeof h - 1] = 0;
    for (char* p = h; *p; p++) if (*p == '\n') *p = ' ';
    strcpy_P(nm, (PGM_P)pgm_read_word(&TNAME[t]));
    strcpy_P(rs, (PGM_P)pgm_read_word(&RSTAT[tRes[t]]));
    DBG("%2u %-5s %-3s %-10s %s\n", t + 1, nm, rs, d, h);
    if (tRes[t] == R_FAIL) nf++;
    if (tRes[t] == R_WARN) nw++;
  }
  DBG("result: %u fail, %u warn.  %umA (CS %u vsense %u = %umA) 1/%u %S %S\n", nf, nw, S.mA, gCS, gVs,
      actualmA(), 1 << S.mres, S.spread ? PSTR("spread") : PSTR("stealth"), S.stepDrv ? PSTR("STEP") : PSTR("UART"));
}

// ================================================================ coil check
// SpreadCycle at 30 rpm (1/16) for 2 s while sampling the open-load and
// short flags. 30 rpm = 100 full steps/s, slow enough for the driver's
// open-load detection and well clear of back-EMF effects.
void coilCheck() {
  if (!tmcOk) { toast(PSTR("No driver link")); return; }
  if (vmMv < VM_OK_MV) { toast(PSTR("Needs 12V on VM")); return; }
  motorKill();
  fault = F_NONE;
  showBusy(PSTR("Coil check, 2 s"));
  coilN = coilA = coilB = coilShort = coilOt = 0;
  coilmA = actualmA();
  tmcWrite(R_GCONF, gconfVal(1));
  tmcWrite(R_CHOPCONF, chopVal(4, 3));
  tmcWrite(R_IHOLD_IRUN, iholdVal(true));
  tmcWrite(R_GSTAT, 7);
  EN_ON();
  delay(60);
  tmcWrite(R_VACTUAL, 2237);                           // 30 rpm: 1600 usteps/s * 1.3981
  LED_ON();
  uint32_t t0 = millis();
  while (millis() - t0 < 2000) {
    uint32_t ds;
    wdt_reset();
    if (tmcRead(R_DRV_STATUS, &ds) == TE_OK) {
      coilN++;
      if (ds & DS_OLA) coilA++;
      if (ds & DS_OLB) coilB++;
      coilShort |= (uint8_t)((ds >> 2) & 0x0F);
      if (ds & DS_OT) coilOt = 1;
      if (ds & DS_SHORT) break;
    }
    delay(30);
  }
  tmcWrite(R_VACTUAL, 0);
  delay(50);
  EN_OFF();
  LED_OFF();
  applyConfig();
  DBG("coil check: %u samples, open A %u, open B %u, short %X, ot %u, %umA\n", coilN, coilA, coilB, coilShort, coilOt, coilmA);
  screen = SC_COIL;
  tDraw = 0;
}

// ================================================================ driver polling and faults
void pollDriver(uint32_t now) {
  static uint8_t n;
  uint32_t v;
  n++;
  if (!tmcOk) {                                        // try to get the link back once a second
    if ((n % 10) == 0 && vmMv >= VM_OK_MV && tmcRead(R_IFCNT, &v) == TE_OK && applyConfig()) {
      tmcOk = true;
      linkFails = 0;
      if (fault == F_LINK) fault = F_NONE;
      toast(PSTR("Driver link back"));
    }
    return;
  }
  if (tmcRead(R_DRV_STATUS, &v) == TE_OK) { drvStatus = v; linkFails = 0; }
  else if (++linkFails >= 3) { tmcOk = false; if (mState != M_OFF) setFault(F_LINK); else EN_OFF(); return; }
  if (mState == M_RUN || mState == M_STOP) {
    if (tmcRead(R_SG_RESULT, &v) == TE_OK) sgResult = (uint16_t)v;
    if (tmcRead(R_PWM_SCALE, &v) == TE_OK) pwmSum = (uint8_t)v;
  }
  if ((n % 5) == 0 && tmcRead(R_GSTAT, &v) == TE_OK && (v & 1)) {
    // driver reset itself (VM dip): it came back on pot current and 1/8 steps
    drvResets++;
    bool was = mState != M_OFF;
    motorKill();
    tmcWrite(R_GSTAT, 7);
    applyConfig();
    DBG("driver reset detected (%u), config re-applied\n", drvResets);
    if (was) fault = F_DRV;
  }
  if (mState == M_OFF) return;
  if (drvStatus & DS_OT) setFault(F_OT);
  else if (drvStatus & DS_SHORT) setFault(F_SHORT);
  else if (vmMv < VM_RUN_MV) setFault(F_VM);
  else if (S.stall && !S.spread && mState == M_RUN && now - steadyT > 300 && labs(spNow) >= 3000) {
    if (sgResult < S.stall) { if (++sgLowCount >= 2) setFault(F_STALL); }
    else sgLowCount = 0;
  }
}

// ================================================================ screens
void composePost() {
  char d[12], nm[6], rs[4], h[44];
  uint8_t nf = 0, nw = 0;
  for (uint8_t t = 0; t < T_N; t++) { if (tRes[t] == R_FAIL) nf++; if (tRes[t] == R_WARN) nw++; }
  if (!nf && !nw) putP(0, 0, PSTR("SELF TEST     ALL OK"));
  else putF(0, 0, PSTR("TEST  %u FAIL  %u WARN"), nf, nw);
  invRow(0);
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t t = top + i;
    if (t >= T_N) break;
    strcpy_P(nm, (PGM_P)pgm_read_word(&TNAME[t]));
    strcpy_P(rs, (PGM_P)pgm_read_word(&RSTAT[tRes[t]]));
    testDetail(t, d);
    putF(1 + i, 0, PSTR("%2u %-4s %s %s"), t + 1, nm, rs, d);
    if (t == cur) invRow(1 + i);
  }
  strncpy_P(h, testHint(cur), sizeof h);
  h[sizeof h - 1] = 0;
  char* nl = strchr(h, '\n');
  if (nl) { *nl = 0; put(7, 0, nl + 1); }
  put(6, 0, h);
}

void fmtRpm(char* b, int32_t c) {
  int32_t a = c < 0 ? -c : c;
  const char* sg = c < 0 ? "-" : "";
  if (a < 100000L) sprintf_P(b, PSTR("%s%ld.%ld"), sg, a / 100, (a / 10) % 10);
  else sprintf_P(b, PSTR("%s%ld"), sg, a / 100);
}

PGM_P faultText(uint8_t f) {
  switch (f) {
    case F_OT:    return PSTR("OVERTEMP");
    case F_SHORT: return PSTR("OUTPUT SHORT");
    case F_VM:    return PSTR("VM LOW");
    case F_LINK:  return PSTR("DRIVER LINK LOST");
    case F_STALL: return PSTR("STALL");
    case F_DRV:   return PSTR("DRIVER RESET");
  }
  return PSTR("");
}

void composeMain() {
  char b[16];
  bool run = mState != M_OFF;
  // row 0: state, microsteps, chopper, drive, VM
  putP(0, 0, fault ? PSTR("FLT") : run ? PSTR("RUN") : PSTR("STOP"));
  if (S.mres) putF(0, 5, PSTR("1/%u"), 1 << S.mres); else putP(0, 5, PSTR("full"));
  putP(0, 11, S.spread ? PSTR("SPR") : PSTR("STL"));
  uint16_t v100 = (uint16_t)((vmMv + 50) / 100);
  sprintf_P(b, PSTR("%u.%uV"), v100 / 10, v100 % 10);
  putRight(0, b);
  if (run || fault) invRow(0);

  // rows 2-3: big speed (actual while running, setpoint while stopped)
  fmtRpm(bigS, run ? spNow : S.speed);
  bigRow = 2;
  bigX = (uint8_t)(98 - 12 * strlen(bigS));
  putP(3, 17, PSTR("rpm"));
  if (run && spNow != S.speed) { fmtRpm(b, S.speed); putP(1, 0, PSTR("set")); put(1, 4, b); }
  if (!run) putP(1, 0, PSTR("set speed"));
  putP(1, 17, S.stepDrv ? PSTR("STEP") : PSTR("UART"));

  // row 4: current, CS, temperature band
  uint16_t ma = actualmA() + 5;
  uint8_t csA = (uint8_t)((drvStatus >> 16) & 0x1F);
  PGM_P tmp = (drvStatus & 0x800) ? PSTR(">157C") : (drvStatus & 0x400) ? PSTR(">150C") :
              (drvStatus & 0x200) ? PSTR(">143C") : (drvStatus & 0x100) ? PSTR(">120C") : PSTR("<120C");
  putF(4, 0, PSTR("%u.%02uA  CS %2u"), ma / 1000, (ma % 1000) / 10, run ? csA : gCS);
  putP(4, 16, tmp);

  // row 5: StallGuard load bar (StealthChop only). Long bar = light load.
  putP(5, 0, PSTR("SG"));
  uint16_t s = (run && !S.spread) ? (sgResult > 510 ? 510 : sgResult) : 0;
  uint8_t px = (uint8_t)(s * 78UL / 510UL);            // 13 cells x 6 px
  for (uint8_t i = 0; i < 13; i++) tb[5][3 + i] = BAR(px >= 6 ? 6 : px), px = px >= 6 ? px - 6 : 0;
  tb[5][16] = 0x17;
  if (run && !S.spread) { sprintf_P(b, PSTR("%u"), sgResult); putRight(5, b); } else putRight(5, "--");

  // row 6: flags while running
  if (run) {
    uint8_t c = 0;
    if (drvStatus & DS_OLA) { putP(6, c, PSTR("OL-A")); c += 5; }
    if (drvStatus & DS_OLB) { putP(6, c, PSTR("OL-B")); c += 5; }
    if (drvStatus & DS_OTPW) { putP(6, c, PSTR("HOT")); c += 4; }
    if (diagWired && DIAG_HI()) { putP(6, c, PSTR("DIAG")); c += 5; }
    if (!c) putF(6, 0, PSTR("PWM %u%%"), (uint16_t)pwmSum * 100 / 255);
  }

  // row 7: fault or hint
  if (fault) {
    putP(7, 0, faultText(fault));
    putP(7, 14, PSTR("[push]"));
    invRow(7);
  } else if (!tmcOk && !S.stepDrv) putP(7, 0, PSTR("no driver  hold:menu"));
  else putP(7, 0, run ? PSTR("push:stop  hold:menu") : PSTR("push:run   hold:menu"));
}

static const char mi0[] PROGMEM = "Current";
static const char mi1[] PROGMEM = "Microsteps";
static const char mi2[] PROGMEM = "Chopper";
static const char mi3[] PROGMEM = "Drive";
static const char mi4[] PROGMEM = "Accel";
static const char mi5[] PROGMEM = "Steps/rev";
static const char mi6[] PROGMEM = "Stall stop";
static const char mi7[] PROGMEM = "Knob dir";
static const char mi8[] PROGMEM = "Screen";
static const char mi9[] PROGMEM = "VM trim";
static const char mi10[] PROGMEM = "Serial log";
static const char mi11[] PROGMEM = "Coil check";
static const char mi12[] PROGMEM = "Driver regs";
static const char mi13[] PROGMEM = "Self test";
static const char mi14[] PROGMEM = "Defaults";
static const char mi15[] PROGMEM = "Save + exit";
static PGM_P const MNAME[MI_N] PROGMEM = {mi0, mi1, mi2, mi3, mi4, mi5, mi6, mi7, mi8, mi9, mi10, mi11, mi12, mi13, mi14, mi15};

void menuValue(uint8_t i, char* b) {
  PGM_P s = 0;
  b[0] = 0;
  switch (i) {
    case MI_CURR:  sprintf_P(b, PSTR("%umA"), S.mA); break;
    case MI_USTEP: if (S.mres) sprintf_P(b, PSTR("1/%u"), 1 << S.mres); else s = PSTR("full"); break;
    case MI_CHOP:  s = S.spread ? PSTR("Spread") : PSTR("Stealth"); break;
    case MI_DRIVE: s = S.stepDrv ? PSTR("STEP pin") : PSTR("UART"); break;
    case MI_ACCEL: { uint16_t a = pgm_read_word(&ACCEL_TAB[S.accel]); if (a) sprintf_P(b, PSTR("%u/s"), a); else s = PSTR("instant"); } break;
    case MI_SPR:   s = S.spr400 ? PSTR("400 0.9d") : PSTR("200 1.8d"); break;
    case MI_STALL: if (S.stall) sprintf_P(b, PSTR("SG<%u"), S.stall); else s = PSTR("off"); break;
    case MI_KNOB:  s = S.knobRev ? PSTR("reversed") : PSTR("normal"); break;
    case MI_FLIP:  s = S.flip ? PSTR("flipped") : PSTR("normal"); break;
    case MI_TRIM:  sprintf_P(b, PSTR("%c%d.%d%%"), S.trim < 0 ? '-' : '+', abs(S.trim) / 10, abs(S.trim) % 10); break;
    case MI_LOG:   s = S.log ? PSTR("1 Hz") : PSTR("off"); break;
    case MI_EXIT:  break;
    default:       s = PSTR(">"); break;
  }
  if (s) strcpy_P(b, s);
}

void composeMenu() {
  char v[12];
  if (cur == MI_TRIM) putF(0, 0, PSTR("MENU      VM %u.%02uV"), vmMv / 1000, (vmMv % 1000) / 10);
  else if (cur == MI_CURR) putF(0, 0, PSTR("MENU   actual %umA"), actualmA());
  else putP(0, 0, PSTR("MENU"));
  invRow(0);
  for (uint8_t i = 0; i < 7; i++) {
    uint8_t it = top + i;
    if (it >= MI_N) break;
    uint8_t r = 1 + i;
    putP(r, 1, (PGM_P)pgm_read_word(&MNAME[it]));
    menuValue(it, v);
    uint8_t n = (uint8_t)strlen(v);
    put(r, 20 - n, v);
    if (it == cur) { if (editing) invSpan(r, 19 - n, n + 2); else invRow(r); }
  }
}

void composeCoil() {
  putF(0, 0, PSTR("COIL CHECK   %umA"), coilmA);
  invRow(0);
  if (!coilN) { putP(2, 0, PSTR("No status reads")); return; }
  bool oa = coilA * 2 > coilN, ob = coilB * 2 > coilN;
  putF(2, 0, PSTR("Coil A  %-5S %u/%u"), oa ? PSTR("OPEN") : coilA ? PSTR("flaky") : PSTR("ok"), coilA, coilN);
  putF(3, 0, PSTR("Coil B  %-5S %u/%u"), ob ? PSTR("OPEN") : coilB ? PSTR("flaky") : PSTR("ok"), coilB, coilN);
  if (coilShort) putF(4, 0, PSTR("SHORT %S%S%S%S"), (coilShort & 1) ? PSTR("A-G ") : PSTR(""), (coilShort & 2) ? PSTR("B-G ") : PSTR(""),
                      (coilShort & 4) ? PSTR("A-V ") : PSTR(""), (coilShort & 8) ? PSTR("B-V") : PSTR(""));
  else putP(4, 0, coilOt ? PSTR("Overtemp!") : PSTR("Shorts  none"));
  PGM_P v;
  if (coilShort) v = PSTR("Fix the wiring first");
  else if (oa && ob) v = PSTR("No motor, or coil\npairs mixed (A1/B1)");
  else if (oa) v = PSTR("Coil A open: wire,\ncrimp or winding");
  else if (ob) v = PSTR("Coil B open: wire,\ncrimp or winding");
  else v = PSTR("Motor OK. Both coils\ncarry current");
  char h[44];
  strncpy_P(h, v, sizeof h);
  h[sizeof h - 1] = 0;
  char* nl = strchr(h, '\n');
  if (nl) { *nl = 0; put(7, 0, nl + 1); }
  put(6, 0, h);
}

static const char rg0[] PROGMEM = "GCONF";
static const char rg1[] PROGMEM = "GSTAT";
static const char rg2[] PROGMEM = "IFCNT";
static const char rg3[] PROGMEM = "IOIN";
static const char rg4[] PROGMEM = "TSTEP";
static const char rg5[] PROGMEM = "SG_RESULT";
static const char rg6[] PROGMEM = "MSCNT";
static const char rg7[] PROGMEM = "CHOPCONF";
static const char rg8[] PROGMEM = "DRV_STATUS";
static const char rg9[] PROGMEM = "PWMCONF";
static const char rg10[] PROGMEM = "PWM_SCALE";
static const char rg11[] PROGMEM = "PWM_AUTO";
static PGM_P const RGN[12] PROGMEM = {rg0, rg1, rg2, rg3, rg4, rg5, rg6, rg7, rg8, rg9, rg10, rg11};
static const uint8_t RGA[12] PROGMEM = {R_GCONF, R_GSTAT, R_IFCNT, R_IOIN, R_TSTEP, R_SG_RESULT, R_MSCNT,
                                        R_CHOPCONF, R_DRV_STATUS, R_PWMCONF, R_PWM_SCALE, R_PWM_AUTO};

void readRegs() {
  regOk = 0;
  if (!tmcOk) return;
  for (uint8_t i = 0; i < 12; i++)
    if (tmcRead(pgm_read_byte(&RGA[i]), &regVal[i]) != TE_OK) return;
  regOk = 1;
}

void composeRegs() {
  putF(0, 0, PSTR("DRIVER REGS  addr %u"), tmcAddr);
  invRow(0);
  if (!regOk) { putP(2, 0, tmcOk ? PSTR("read failed") : PSTR("no driver link")); return; }
  for (uint8_t i = 0; i < 7; i++) {
    uint8_t r = top + i;
    if (r >= 12) break;
    putP(1 + i, 0, (PGM_P)pgm_read_word(&RGN[r]));
    putF(1 + i, 12, PSTR("%08lX"), regVal[r]);
  }
}

void compose() {
  clr();
  switch (screen) {
    case SC_POST: composePost(); break;
    case SC_MAIN: composeMain(); break;
    case SC_MENU: composeMenu(); break;
    case SC_COIL: composeCoil(); break;
    case SC_REGS: composeRegs(); break;
  }
  if (toastMsg) {
    if ((int32_t)(millis() - toastUntil) > 0) toastMsg = 0;
    else {
      memset(tb[7], ' ', 21);
      uint8_t n = (uint8_t)strlen_P(toastMsg);
      putP(7, n < 21 ? (21 - n) / 2 : 0, toastMsg);
      invRow(7);
    }
  }
}

// Builds a frame every 100 ms, then sends one changed row per loop pass
// (~3 ms each at 400 kHz) so the loop never stalls on the display.
void displayTask(uint32_t now) {
  static uint8_t r = 8;
  static uint32_t tFull;
  if (!oledOk) return;
  if (r >= 8) {
    if (now - tDraw < 100) return;
    tDraw = now;
    if (screen == SC_REGS) readRegs();
    compose();
    if (now - tFull > 3000) { tFull = now; memset(rowSum, 0, sizeof rowSum); }   // periodic full refresh
    r = 0;
  }
  for (; r < 8; r++) {
    uint16_t h = rowHash(r);
    if (h == rowSum[r]) continue;
    if (oledRow(r)) { rowSum[r] = h; oledErrs = 0; }
    else if (++oledErrs > 20) { oledBegin(); oledErrs = 0; if (!oledOk) DBG("OLED lost\n"); }
    r++;
    return;
  }
}

// ================================================================ menu actions
bool needStop() {
  if (mState == M_OFF) return false;
  motorKill();
  toast(PSTR("Motor stopped"));
  return true;
}

void menuAdjust(uint8_t i, int8_t d) {
  switch (i) {
    case MI_CURR: {
      int16_t m = (int16_t)S.mA + 50 * d;
      S.mA = (uint16_t)(m < 100 ? 100 : (m > 1750 ? 1750 : m));
      calcCurrent();
      if (tmcOk) { tmcWrite(R_CHOPCONF, chopVal(S.mres, 3)); tmcWrite(R_IHOLD_IRUN, iholdVal(false)); }
    } break;
    case MI_USTEP: {
      int8_t m = (int8_t)S.mres + d;
      S.mres = (uint8_t)(m < 0 ? 0 : (m > 8 ? 8 : m));
      needStop();
      if (tmcOk) tmcWrite(R_CHOPCONF, chopVal(S.mres, 3));
    } break;
    case MI_CHOP:  S.spread ^= 1; needStop(); if (tmcOk) tmcWrite(R_GCONF, gconfVal(S.spread)); break;
    case MI_DRIVE: S.stepDrv ^= 1; needStop(); break;
    case MI_ACCEL: { int8_t a = (int8_t)S.accel + d; S.accel = (uint8_t)(a < 0 ? 0 : (a >= N_ACCEL ? N_ACCEL - 1 : a)); } break;
    case MI_SPR:   S.spr400 ^= 1; needStop(); break;
    case MI_STALL: { int16_t s = (int16_t)S.stall + 10 * d; S.stall = (uint8_t)(s < 0 ? 0 : (s > 250 ? 250 : s)); } break;
    case MI_KNOB:  S.knobRev ^= 1; break;
    case MI_FLIP:  S.flip ^= 1; oledFlip(); memset(rowSum, 0, sizeof rowSum); break;
    case MI_TRIM:  { int16_t t = S.trim + d; S.trim = (int8_t)(t < -100 ? -100 : (t > 100 ? 100 : t)); } break;
    case MI_LOG:   S.log ^= 1; break;
  }
}

void menuSelect(uint8_t i) {
  switch (i) {
    case MI_COIL: coilCheck(); break;
    case MI_REGS: screen = SC_REGS; top = 0; break;
    case MI_POST: saveSettings(); runPost(); break;
    case MI_DEF:  { int32_t sp = S.speed; setDefaults(); S.speed = sp; calcCurrent(); needStop();
                    if (tmcOk) applyConfig(); oledFlip(); memset(rowSum, 0, sizeof rowSum);
                    toast(PSTR("Defaults loaded")); } break;
    case MI_EXIT: saveSettings(); screen = SC_MAIN; break;
    default:      editing = 1; break;
  }
}

// ================================================================ UI input
int32_t clampSpeed(int32_t n) { int32_t m = maxCrpm(); return n > m ? m : (n < -m ? -m : n); }

// Coarse: next "nice" speed (0.5, 1, 2, 5, 10 ... 2000 rpm), through zero to reverse.
int32_t niceStep(int32_t v, int8_t dir) {
  if (v == 0) return dir > 0 ? 50 : -50;
  bool neg = v < 0;
  int32_t a = neg ? -v : v, r = 0;
  if ((dir > 0) != neg) {                              // away from zero: next larger
    r = (int32_t)pgm_read_word(&NICE[N_NICE - 1]) * 10;
    for (uint8_t i = 0; i < N_NICE; i++) {
      int32_t n = (int32_t)pgm_read_word(&NICE[i]) * 10;
      if (n > a) { r = n; break; }
    }
  } else {                                             // toward zero: next smaller, then 0
    for (int8_t i = N_NICE - 1; i >= 0; i--) {
      int32_t n = (int32_t)pgm_read_word(&NICE[i]) * 10;
      if (n < a) { r = n; break; }
    }
  }
  return neg ? -r : r;
}

void speedTurn(int8_t d, bool fine) {
  int32_t v = S.speed;
  if (fine) {                                          // largest 1-2-5 step that is <= 2 % of the speed, 0.1 rpm minimum
    int32_t a = v < 0 ? -v : v, s = 10;
    for (;;) {
      int32_t nx = (s == 20 || s == 200 || s == 2000) ? s * 5 / 2 : s * 2;   // 10 20 50 100 200 500 ...
      if (nx * 50 > a) break;
      s = nx;
    }
    int32_t n = v + d * s;
    if ((v > 0 && n < 0) || (v < 0 && n > 0)) n = 0;   // stop at zero before reversing
    v = n;
  } else {
    for (int8_t k = d < 0 ? -d : d; k > 0; k--) v = niceStep(v, d > 0 ? 1 : -1);
  }
  S.speed = clampSpeed(v);
}

void uiInput(int8_t d, uint8_t b) {
  tDraw = 0;
  switch (screen) {
    case SC_POST:
      if (d) { int8_t c = (int8_t)cur + d; cur = (uint8_t)(c < 0 ? 0 : (c >= T_N ? T_N - 1 : c));
               if (cur < top) top = cur;
               if (cur > top + 4) top = cur - 4; }
      if (b) screen = SC_MAIN;
      break;
    case SC_MAIN:
      if (d) speedTurn(d, btnDown);
      if (b == 1) {
        if (fault) { fault = F_NONE; if (tmcOk) tmcWrite(R_GSTAT, 7); }
        else if (mState == M_OFF) motorStart();
        else motorStop();
      } else if (b == 2) { screen = SC_MENU; cur = 0; top = 0; editing = 0; }
      break;
    case SC_MENU:
      if (editing) {
        if (d) menuAdjust(cur, d);
        if (b == 1) editing = 0;
        else if (b == 2) { editing = 0; saveSettings(); screen = SC_MAIN; }
      } else {
        if (d) { int8_t c = (int8_t)cur + d; cur = (uint8_t)(c < 0 ? 0 : (c >= MI_N ? MI_N - 1 : c));
                 if (cur < top) top = cur;
                 if (cur > top + 6) top = cur - 6; }
        if (b == 1) menuSelect(cur);
        else if (b == 2) { saveSettings(); screen = SC_MAIN; }
      }
      break;
    case SC_COIL:
      if (b) screen = SC_MENU;
      break;
    case SC_REGS:
      if (d) { int8_t t = (int8_t)top + d; top = (uint8_t)(t < 0 ? 0 : (t > 5 ? 5 : t)); }
      if (b) { screen = SC_MENU; top = cur > 6 ? cur - 6 : 0; }
      break;
  }
}

// ================================================================ LED
void ledTask(uint32_t now) {
  static uint32_t t0;
  bool on;
  if (fault) on = (now / 60) & 1;
  else if (mState != M_OFF) on = true;
  else if (postFirstFail && screen == SC_POST) {
    uint32_t cyc = (uint32_t)postFirstFail * 500UL + 1500UL;   // N x (250 on, 250 off), 1.5 s dark
    uint32_t ph = now - t0;
    if (ph >= cyc) { t0 = now; ph = 0; }
    on = ph < (uint32_t)postFirstFail * 500UL && (ph % 500UL) < 250UL;
  } else on = (now % 2000UL) < 40UL;
  if (on) LED_ON(); else LED_OFF();
}

// ================================================================ setup / loop
void setup() {
  // Outputs to a safe state first: driver off, no step pulses, LED on
  PORTD = _BV(PD7) | _BV(PD5) | _BV(PD4) | _BV(PD3) | _BV(PD2);  // EN high, LED on, knob pull-ups
  DDRD  = _BV(PD7) | _BV(PD6) | _BV(PD5);
  PORTB = _BV(PB2) | _BV(PB0) | _BV(PB3) | _BV(PB4) | _BV(PB5);  // debug idle high, DIAG + unused pulled up
  DDRB  = _BV(PB1) | _BV(PB2);
  PORTC = _BV(PC2) | _BV(PC3);                                   // spare J9 pins pulled up
  DDRC  = _BV(PC1);                                              // buzzer transistor off
  DIDR0 = _BV(ADC0D);                                            // VM sense pin: analog only
  uartInit();

  DBG("\nStepper Tester fw " FW_VER " (" __DATE__ ")  reset %02X\n", resetCause);

  bool stored = loadSettings();
  if (SW_DOWN()) {                                               // held at power-up: defaults
    uint32_t t0 = millis();
    while (SW_DOWN() && millis() - t0 < 4000) { }
    if (!SW_DOWN()) { setDefaults(); saveSettings(); stored = true; DBG("knob held at power-up: defaults restored\n"); }
  }
  if (!stored) DBG("no valid settings stored, using defaults\n");
  calcCurrent();
  inputsInit();
  wdt_enable(WDTO_2S);
  runPost();
}

void loop() {
  static uint32_t tPoll, tLog;
  wdt_reset();
  uint32_t now = millis();

  int8_t d = takeEnc();
  uint8_t b = takeBtn();
  if (d || b) uiInput(d, b);

  motorTask(now);

  if (now - tPoll >= 100) {
    tPoll = now;
    vmMv = readVM();
    pollDriver(now);
    // 12 V showed up after a self test that ran without it: test again
    if (vmWasAbsent && mState == M_OFF && (screen == SC_POST || screen == SC_MAIN)) {
      if (vmMv >= VM_OK_MV) { if (++vmSeen >= 5) { vmSeen = 0; runPost(); } }
      else vmSeen = 0;
    }
  }

  ledTask(now);
  displayTask(now);

  if (S.log && now - tLog >= 1000) {
    tLog = now;
    char r[12];
    fmtRpm(r, spNow);
    DBG("t=%lu st=%u rpm=%s VM=%u DS=%08lX SG=%u PWM=%u flt=%u\n", now / 1000, mState, r, vmMv, drvStatus, sgResult, pwmSum, fault);
  }
}
