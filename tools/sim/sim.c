// Board-level simulator for the stepper tester firmware.
// Runs the real compiled ELF in simavr (ATmega328P @ 16 MHz) with:
//   - a TMC2209 model on the single-wire UART (echo + datagrams + registers)
//   - an SSD1306 I2C model (RAM dumped to .pgm files for viewing)
//   - VM on ADC0 through the 10k/1.8k divider, AVCC 5.000 V
//   - encoder A/B/switch on PD2/PD3/PD4, DIAG on PB0
//   - a decoder for the bit-banged debug log on PB2 (timing checked)
//   - a STEP pulse counter on OC1A
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <simavr/sim_avr.h>
#include <simavr/sim_elf.h>
#include <simavr/sim_irq.h>
#include <simavr/avr_uart.h>
#include <simavr/avr_twi.h>
#include <simavr/avr_adc.h>
#include <simavr/avr_ioport.h>
#include <simavr/avr_timer.h>

static avr_t *avr;
static const char *out_dir = "out";                  // screen dumps and logs; SIM_OUT overrides
static long led_toggles; static int led_lv; static avr_cycle_count_t led_t[64]; static int led_n;
static avr_irq_t *uin, *twi_in;
#define F 16000000ULL
#define MS(x) ((avr_cycle_count_t)((x) * (F / 1000ULL)))

// ------------------------------------------------------------- scenario knobs
static struct {
  int vm_mv, tmc_present, tmc_addr, echo, diag_wired, en_broken, dir_broken, step_broken;
  int motor, short_flags, oled, spread_pin, version, verbose_uart;
} sc = {12000, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0x21, 0};

// ------------------------------------------------------------- pins
static int out_level(int port_addr_ddr, int bit) {   // level the MCU drives (inputs -> pulled up)
  uint8_t ddr = avr->data[port_addr_ddr], prt = avr->data[port_addr_ddr + 1];
  if (ddr & (1 << bit)) return (prt >> bit) & 1;
  return 1;
}
#define DDRB_A 0x24
#define DDRD_A 0x2A
static uint8_t extB_mask, extB_val, extC_mask, extC_val, extD_mask, extD_val;
static void ext_apply(char port) {
  avr_ioport_external_t e;
  e.name = port;
  if (port == 'B') { e.mask = extB_mask; e.value = extB_val; }
  if (port == 'C') { e.mask = extC_mask; e.value = extC_val; }
  if (port == 'D') { e.mask = extD_mask; e.value = extD_val; }
  avr_ioctl(avr, AVR_IOCTL_IOPORT_SET_EXTERNAL(port), &e);
}
static void drive(char port, int bit, int v) {       // external drive of an input pin
  uint8_t *m = port == 'B' ? &extB_mask : port == 'C' ? &extC_mask : &extD_mask;
  uint8_t *val = port == 'B' ? &extB_val : port == 'C' ? &extC_val : &extD_val;
  *m |= 1 << bit;
  if (v) *val |= 1 << bit; else *val &= ~(1 << bit);
  ext_apply(port);
  avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(port), bit), v);
}
static void release(char port, int bit) {
  uint8_t *m = port == 'B' ? &extB_mask : port == 'C' ? &extC_mask : &extD_mask;
  *m &= ~(1 << bit);
  ext_apply(port);
}

// ------------------------------------------------------------- TMC2209 model
static uint32_t treg[128];
static uint8_t ifcnt, gstat = 1, rxb[8];
static int rxn, nreads, nwrites, crcerr, fe_sent;
static int vact_log_n; static long vact_log_t[4096]; static int32_t vact_log_v[4096];

static uint8_t crc8(const uint8_t *d, int n) {
  uint8_t crc = 0;
  for (int i = 0; i < n; i++) {
    uint8_t b = d[i];
    for (int j = 0; j < 8; j++) {
      if ((crc >> 7) ^ (b & 1)) crc = (uint8_t)((crc << 1) ^ 0x07); else crc <<= 1;
      b >>= 1;
    }
  }
  return crc;
}
static int tmc_powered(void) { return sc.tmc_present && sc.vm_mv > 5500; }
static int diag_level(void) {
  int en_pin = sc.en_broken ? 1 : out_level(DDRD_A, 7);
  return en_pin ? 0 : (sc.short_flags ? 1 : 0);      // DIAG high only on a driver error while enabled
}
static uint32_t ioin(void) {
  int en = sc.en_broken ? 1 : out_level(DDRD_A, 7);
  int dir = sc.dir_broken ? 0 : out_level(DDRD_A, 6);
  int step = sc.step_broken ? 0 : out_level(DDRB_A, 1);
  return (uint32_t)en | ((sc.tmc_addr & 1) << 2) | (((sc.tmc_addr >> 1) & 1) << 3) | (diag_level() << 4) |
         (1 << 6) | (step << 7) | (sc.spread_pin << 8) | (dir << 9) | ((uint32_t)sc.version << 24);
}
static uint32_t drv_status(void) {
  int en_pin = sc.en_broken ? 1 : out_level(DDRD_A, 7);
  int enabled = !en_pin && (treg[0x6C] & 0xF) != 0;
  uint32_t ds = 0;
  if (enabled) {
    ds |= ((treg[0x10] >> 8) & 0x1F) << 16;
    if (sc.motor == 0) ds |= 0xC0;
    else if (sc.motor == 2) ds |= 0x40;
    ds |= (uint32_t)sc.short_flags << 2;
    if (sc.short_flags) gstat |= 2;
  }
  if (treg[0x22] == 0) ds |= 0x80000000u;
  if (!(treg[0] & 4)) ds |= 0x40000000u;
  return ds;
}
static uint32_t tmc_read(uint8_t reg) {
  switch (reg) {
    case 0x01: return gstat;
    case 0x02: return ifcnt;
    case 0x06: return ioin();
    case 0x12: return 0xFFFFF;
    case 0x41: return (treg[0x22] && !(treg[0] & 4)) ? 240 : 0;
    case 0x6A: return 0x200;
    case 0x6F: return drv_status();
    case 0x71: return 0x00700050;
    case 0x72: return 0x000E0024;
    case 0x00: return treg[0] & 0x3FF;
    default:   return treg[reg];
  }
}
static void tmc_write(uint8_t reg, uint32_t v) {
  if (reg == 0x01) { gstat &= ~v; return; }
  treg[reg] = v;
  if (reg == 0x22) {
    int32_t s = (int32_t)(v << 8) >> 8;
    if (vact_log_n < 4096) { vact_log_t[vact_log_n] = (long)(avr->cycle / (F / 1000)); vact_log_v[vact_log_n++] = s; }
  }
}
static void tmc_byte(uint8_t b) {
  if (rxn == 0 && b != 0x05) return;                  // wait for the sync byte
  rxb[rxn++] = b;
  if (rxn == 4 && !(rxb[2] & 0x80)) {
    rxn = 0;
    if (crc8(rxb, 3) != rxb[3]) { crcerr++; return; }
    if (!tmc_powered() || rxb[1] != sc.tmc_addr) return;
    uint32_t v = tmc_read(rxb[2]);
    uint8_t r[8] = {0x05, 0xFF, rxb[2], (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v, 0};
    r[7] = crc8(r, 7);
    if (sc.echo == 1) for (int i = 0; i < 8; i++) avr_raise_irq(uin, r[i]);
    nreads++;
  } else if (rxn == 8) {
    rxn = 0;
    if (crc8(rxb, 7) != rxb[7]) { crcerr++; return; }
    if (!tmc_powered() || rxb[1] != sc.tmc_addr) return;
    tmc_write(rxb[2] & 0x7F, ((uint32_t)rxb[3] << 24) | ((uint32_t)rxb[4] << 16) | ((uint32_t)rxb[5] << 8) | rxb[6]);
    ifcnt++;
    nwrites++;
  }
}
static void uart_tx_hook(struct avr_irq_t *irq, uint32_t value, void *param) {
  (void)irq; (void)param;
  uint8_t b = (uint8_t)value;
  if (sc.verbose_uart) printf("  [uart %8.3f ms] %02X\n", avr->cycle / 16000.0, b);
  if (sc.echo == 1) avr_raise_irq(uin, b);           // single wire: TX comes straight back
  else if (sc.echo == 2) avr_raise_irq(uin, b ^ 0x5A);
  else if (sc.echo == 3) { if (!fe_sent) { avr_raise_irq(uin, UART_INPUT_FE); fe_sent = 1; } }
  if (sc.echo != 3) tmc_byte(b);
}

// ------------------------------------------------------------- SSD1306 model
static uint8_t ram[8][128];
static int o_sel, o_ctrl, o_data, o_cs = 0, o_ce = 127, o_ps = 0, o_pe = 7, o_col, o_pg, o_on, o_flipA = -1, o_flipC = -1;
static int o_need, o_cmd, o_argi, o_args[3], o_rows_written, o_cmds;
static void oled_cmd(uint8_t d) {
  if (o_need) {
    o_args[o_argi++] = d;
    if (--o_need == 0) {
      if (o_cmd == 0x21) { o_cs = o_args[0] & 127; o_ce = o_args[1] & 127; o_col = o_cs; }
      if (o_cmd == 0x22) { o_ps = o_args[0] & 7; o_pe = o_args[1] & 7; o_pg = o_ps; }
    }
    return;
  }
  o_cmds++;
  o_cmd = d; o_argi = 0;
  switch (d) {
    case 0x21: case 0x22: o_need = 2; break;
    case 0x81: case 0xA8: case 0xD3: case 0xD5: case 0xD9: case 0xDA: case 0xDB: case 0x8D: case 0x20: o_need = 1; break;
    case 0xAF: o_on = 1; break;
    case 0xAE: o_on = 0; break;
    case 0xA0: case 0xA1: o_flipA = d; break;
    case 0xC0: case 0xC8: o_flipC = d; break;
  }
}
static void twi_hook(struct avr_irq_t *irq, uint32_t value, void *param) {
  (void)irq; (void)param;
  avr_twi_msg_irq_t v;
  v.u.v = value;
  if (v.u.twi.msg & TWI_COND_STOP) o_sel = 0;
  if (v.u.twi.msg & TWI_COND_START) {
    o_sel = 0;
    if (sc.oled && (v.u.twi.addr >> 1) == 0x3C) {
      o_sel = 1; o_ctrl = 0;
      avr_raise_irq(twi_in, avr_twi_irq_msg(TWI_COND_ACK, v.u.twi.addr, 1));
    }
  }
  if (o_sel && (v.u.twi.msg & TWI_COND_WRITE)) {
    avr_raise_irq(twi_in, avr_twi_irq_msg(TWI_COND_ACK, v.u.twi.addr, 1));
    uint8_t d = v.u.twi.data;
    if (!o_ctrl) { o_ctrl = 1; o_data = (d & 0x40) != 0; return; }
    if (o_data) {
      ram[o_pg][o_col] = d;
      if (o_col == 127 && o_pg >= 0) o_rows_written++;
      if (++o_col > o_ce) { o_col = o_cs; if (++o_pg > o_pe) o_pg = o_ps; }
    } else oled_cmd(d);
  }
}
static void dump_screen(const char *name) {
  char fn[512];
  snprintf(fn, sizeof fn, "%s/%s.pgm", out_dir, name);
  FILE *f = fopen(fn, "wb");
  if (!f) return;
  fprintf(f, "P5\n128 64\n255\n");
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 128; x++) fputc(((ram[y >> 3][x] >> (y & 7)) & 1) ? 255 : 0, f);
  fclose(f);
}

// ------------------------------------------------------------- debug TX (PB2) decoder
static int dbg_level = 1;
static avr_cycle_count_t dbg_t0;
static int dbg_bit = -1;
static uint8_t dbg_byte;
static char dbg_line[512];
static int dbg_len;
static double bit_cyc = 16000000.0 / 57600.0;
static long edges_n;
static double edge_err_max;
static FILE *dbgout;
static avr_cycle_count_t last_edge;
static void dbg_sample_to(avr_cycle_count_t now) {
  // sample all bit centers that fall before 'now' using the level held since last edge
  while (dbg_bit >= 0) {
    double tc = dbg_t0 + (dbg_bit + 1.5) * bit_cyc;
    if (tc >= now) break;
    if (dbg_bit < 8) { if (dbg_level) dbg_byte |= 1 << dbg_bit; dbg_bit++; }
    else {                                             // stop bit sample
      if (!dbg_level) fprintf(dbgout, "<FRAMING ERR>");
      char c = (char)dbg_byte;
      if (c == '\n') { dbg_line[dbg_len] = 0; fprintf(dbgout, "%s\n", dbg_line); dbg_len = 0; }
      else if (c != '\r' && dbg_len < 510) dbg_line[dbg_len++] = c;
      dbg_bit = -1;
    }
  }
}
static void pb2_hook(struct avr_irq_t *irq, uint32_t value, void *param) {
  (void)irq; (void)param;
  int lv = value ? 1 : 0;
  avr_cycle_count_t now = avr->cycle;
  if (lv == dbg_level) return;
  dbg_sample_to(now);
  if (dbg_bit >= 0) {                                  // edge inside a frame: check it lands on a bit boundary
    double k = (now - dbg_t0) / bit_cyc;
    double err = (k - (double)(long)(k + 0.5)) * bit_cyc;
    if (err < 0) err = -err;
    if (err > edge_err_max) edge_err_max = err;
    edges_n++;
  }
  dbg_level = lv;
  if (!lv && dbg_bit < 0) { dbg_t0 = now; dbg_bit = 0; dbg_byte = 0; }
  last_edge = now;
}

// ------------------------------------------------------------- status LED (PD5)
static void pd5_hook(struct avr_irq_t *irq, uint32_t value, void *param) {
  (void)irq; (void)param;
  int lv = value & 1;
  if (lv && !led_lv && led_n < 64) led_t[led_n++] = avr->cycle;
  if (lv != led_lv) led_toggles++;
  led_lv = lv;
}

// ------------------------------------------------------------- STEP pulses on OC1A
static long step_rises;
static avr_cycle_count_t step_first, step_last;
static int step_lv;
static void oc1a_hook(struct avr_irq_t *irq, uint32_t value, void *param) {
  (void)irq; (void)param;
  int lv = value & 1;
  if (lv && !step_lv) { if (!step_rises) step_first = avr->cycle; step_last = avr->cycle; step_rises++; }
  step_lv = lv;
}

// ------------------------------------------------------------- run helpers
static void set_vm(int mv) {
  sc.vm_mv = mv;
  avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_ADC0), (uint32_t)(mv * 1800 / 11800));
}
static double pc_trace_ms = 0;
static uint16_t sp_min = 0xFFFF;

static void run_to(double ms) {
  avr_cycle_count_t until = MS(ms);
  while (avr->cycle < until) {
    uint16_t sp = avr->data[0x5D] | (avr->data[0x5E] << 8);
    if (sp > 0x100 && sp < sp_min) sp_min = sp;
    if (pc_trace_ms > 0 && avr->cycle >= MS(pc_trace_ms)) { printf("  PC trace %.0f ms: 0x%04x UCSR0A=%02X B=%02X C=%02X UBRR=%02X%02X\n", pc_trace_ms, avr->pc, avr->data[0xC0], avr->data[0xC1], avr->data[0xC2], avr->data[0xC5], avr->data[0xC4]); pc_trace_ms += 50; }
    int st = avr_run(avr);
    if (st == cpu_Done || st == cpu_Crashed) { printf("CPU state %d at %.1f ms\n", st, avr->cycle / 16000.0); exit(1); }
  }
}
static void run_for(double ms) { run_to(avr->cycle / 16000.0 + ms); }
static void press(double ms) { drive('D', 4, 0); run_for(ms); drive('D', 4, 1); run_for(60); }
static void turn(int n) {                             // n detents, + = clockwise (A falls first)
  static const int cw[4][2] = {{1, 0}, {0, 0}, {0, 1}, {1, 1}};    // {B, A} after each edge, from rest 11
  static const int ccw[4][2] = {{0, 1}, {0, 0}, {1, 0}, {1, 1}};
  int steps = n > 0 ? n : -n;
  for (int k = 0; k < steps; k++) {
    for (int i = 0; i < 4; i++) {
      const int *st = n > 0 ? cw[i] : ccw[i];
      drive('D', 3, st[0]);
      drive('D', 2, st[1]);
      run_for(2);
    }
    run_for(20);
  }
}

int main(int argc, char **argv) {
  const char *elf = argc > 1 ? argv[1] : "build/minicore/fw.elf";
  const char *scen = argc > 2 ? argv[2] : "happy";
  if (getenv("SIM_OUT")) out_dir = getenv("SIM_OUT");
  double freq = 16000000.0;
  if (!strcmp(scen, "clk8")) freq = 8000000.0;
  elf_firmware_t fw;
  memset(&fw, 0, sizeof fw);
  if (elf_read_firmware(elf, &fw)) { printf("elf read failed\n"); return 1; }
  strcpy(fw.mmcu, "atmega328p");
  fw.frequency = (uint32_t)freq;
  avr = avr_make_mcu_by_name("atmega328p");
  avr_init(avr);
  avr_load_firmware(avr, &fw);
  avr->frequency = (uint32_t)freq;
  avr->avcc = 5000; avr->vcc = 5000; avr->aref = 5000;
  avr->log = 1;

  uint32_t fl = 0;
  avr_ioctl(avr, AVR_IOCTL_UART_GET_FLAGS('0'), &fl);
  fl &= ~(AVR_UART_FLAG_POLL_SLEEP | AVR_UART_FLAG_STDIO);
  avr_ioctl(avr, AVR_IOCTL_UART_SET_FLAGS('0'), &fl);
  uin = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_INPUT);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUTPUT), uart_tx_hook, NULL);

  twi_in = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_OUTPUT), twi_hook, NULL);

  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 2), pb2_hook, NULL);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 5), pd5_hook, NULL);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_TIMER_GETIRQ('1'), TIMER_IRQ_OUT_COMP + 0), oc1a_hook, NULL);

  char dn[512];
  snprintf(dn, sizeof dn, "%s/%s.log", out_dir, scen);
  dbgout = fopen(dn, "w");
  if (!dbgout) { printf("cannot write %s (create the output directory)\n", dn); return 1; }
  setvbuf(dbgout, NULL, _IOLBF, 0);

  // board defaults: encoder at rest (both high), button up, I2C idle high, DIAG per scenario
  drive('D', 2, 1); drive('D', 3, 1); drive('D', 4, 1);
  drive('C', 4, 1); drive('C', 5, 1);

  // scenario setup
  if (!strcmp(scen, "novm")) sc.vm_mv = 0;
  if (!strcmp(scen, "addr2")) sc.tmc_addr = 2;
  if (!strcmp(scen, "noecho")) sc.echo = 0;
  if (!strcmp(scen, "linelow")) sc.echo = 3;
  if (!strcmp(scen, "nodiag")) sc.diag_wired = 0;
  if (!strcmp(scen, "enbroken")) sc.en_broken = 1;
  if (!strcmp(scen, "nooled")) sc.oled = 0;
  if (!strcmp(scen, "short")) sc.short_flags = 0x1;  // s2ga
  if (!strcmp(scen, "nomotor")) sc.motor = 0;
  if (!strcmp(scen, "coilA")) sc.motor = 2;
  if (!strcmp(scen, "notmc")) sc.tmc_present = 0;
  if (!strcmp(scen, "tmc2208")) sc.version = 0x20;
  if (!strcmp(scen, "dirbroken")) sc.dir_broken = 1;
  if (!strcmp(scen, "stepbroken")) sc.step_broken = 1;
  if (getenv("UARTV")) sc.verbose_uart = 1;
  if (getenv("PCT")) pc_trace_ms = atof(getenv("PCT"));
  if (sc.diag_wired) drive('B', 0, 0);                // driver holds DIAG low (EN high)
  set_vm(sc.vm_mv);

  printf("=== scenario %s ===\n", scen);
  if (!strcmp(scen, "holdboot")) drive('D', 4, 0);   // knob held while powering up
  if (!strcmp(scen, "holdboot")) { run_to(1200); drive('D', 4, 1); run_to(2500); }
  run_to(1500);
  snprintf(dn, sizeof dn, "%s_post", scen); dump_screen(dn);
  printf("t=1500ms tmc reads %d writes %d crcerr %d  oled cmds %d on %d\n", nreads, nwrites, crcerr, o_cmds, o_on);

  if (!strcmp(scen, "novm")) {                        // plug the brick in after the first test
    set_vm(12000);
    run_for(2500);
    snprintf(dn, sizeof dn, "%s_post2", scen); dump_screen(dn);
  }
  if (!strcmp(scen, "happy") || !strcmp(scen, "step")) {
    turn(3); run_for(200);
    snprintf(dn, sizeof dn, "%s_post_scrolled", scen); dump_screen(dn);
    press(100); run_for(300);
    snprintf(dn, sizeof dn, "%s_main_stopped", scen); dump_screen(dn);
    if (!strcmp(scen, "step")) {                      // menu: Drive -> STEP pin, then back out
      press(800); run_for(300);
      turn(3); press(100); turn(1); press(100); run_for(300);
      snprintf(dn, sizeof dn, "%s_menu", scen); dump_screen(dn);
      press(800); run_for(300);
    }
    vact_log_n = 0; step_rises = 0;
    press(100);                                       // run at the default 60 rpm
    run_for(1500);
    snprintf(dn, sizeof dn, "%s_main_running", scen); dump_screen(dn);
    printf("start: %d VACTUAL writes", vact_log_n);
    if (vact_log_n) printf(" first %d at %ld ms, last %d at %ld ms", vact_log_v[0], vact_log_t[0], vact_log_v[vact_log_n-1], vact_log_t[vact_log_n-1]);
    printf("; STEP rises %ld", step_rises);
    if (step_rises > 1) printf(" (%.1f Hz avg incl. ramp)", (step_rises - 1) * 16000000.0 / (double)(step_last - step_first));
    printf("; EN %d DIR %d\n", out_level(DDRD_A, 7), out_level(DDRD_A, 6));
    printf("  T1: TCCR1A=%02X TCCR1B=%02X ICR1=%u OCR1A=%u TCNT1=%u TIMSK1=%02X PORTB=%02X DDRB=%02X\n", avr->data[0x80], avr->data[0x81],
           avr->data[0x86] | (avr->data[0x87] << 8), avr->data[0x88] | (avr->data[0x89] << 8), avr->data[0x84] | (avr->data[0x85] << 8), avr->data[0x6F], avr->data[0x25], avr->data[0x24]);
    step_rises = 0; run_for(1000);
    if (step_rises > 1) printf("  steady STEP %.2f Hz (expect 3200.00 for 60 rpm @1/16)\n", (step_rises - 1) * 16000000.0 / (double)(step_last - step_first));
    turn(2); run_for(1500);                           // 60 -> 90 -> 120 rpm
    if (vact_log_n) printf("+2 clicks: VACTUAL %d (expect 8950 for 120 rpm)\n", vact_log_v[vact_log_n - 1]);
    step_rises = 0; run_for(1000);
    if (step_rises > 1) printf("  steady STEP %.2f Hz (expect 6400.00 for 120 rpm)\n", (step_rises - 1) * 16000000.0 / (double)(step_last - step_first));
    // push + turn: fine adjust (button held while turning, no press event)
    drive('D', 4, 0); run_for(50); turn(3); run_for(50); drive('D', 4, 1); run_for(1500);
    if (vact_log_n) printf("push+turn 3: VACTUAL %d (expect 120 + 3*2 = 126 rpm -> 9398)\n", vact_log_v[vact_log_n - 1]);
    turn(-16); run_for(3000);                         // through zero into reverse
    printf("-16 clicks: VACTUAL %d  DIR %d  STEP rises %ld\n", vact_log_n ? vact_log_v[vact_log_n - 1] : 0, out_level(DDRD_A, 6), step_rises);
    snprintf(dn, sizeof dn, "%s_main_reverse", scen); dump_screen(dn);
    press(100); run_for(2000);
    printf("stop: VACTUAL %d, EN %d\n", vact_log_n ? vact_log_v[vact_log_n - 1] : 0, out_level(DDRD_A, 7));
    snprintf(dn, sizeof dn, "%s_main_stopped2", scen); dump_screen(dn);
    press(800); run_for(400);
    snprintf(dn, sizeof dn, "%s_menu_top", scen); dump_screen(dn);
    turn(11); run_for(200); press(100); run_for(3200);
    snprintf(dn, sizeof dn, "%s_coil", scen); dump_screen(dn);
    press(100); run_for(300);
    turn(1); press(100); run_for(800);
    snprintf(dn, sizeof dn, "%s_regs", scen); dump_screen(dn);
    press(100); run_for(300);
    press(800); run_for(300);
    snprintf(dn, sizeof dn, "%s_main_end", scen); dump_screen(dn);
  }
  if (!strcmp(scen, "steplow")) {                   // full step, STEP pin, 1 rpm, then a click up
    press(100); run_for(300);
    press(800); run_for(300);
    turn(1); press(100); turn(-4); press(100);        // Microsteps 1/16 -> full
    turn(2); press(100); turn(1); press(100);         // Drive -> STEP pin
    press(800); run_for(300);
    turn(-8); run_for(300);                           // 60 -> 1 rpm via the coarse list
    snprintf(dn, sizeof dn, "%s_set", scen); dump_screen(dn);
    press(100); run_for(3000);
    step_rises = 0; run_for(3000);
    printf("1 rpm full step: %ld rises in 3 s", step_rises);
    if (step_rises > 1) printf(" -> %.3f Hz (expect 3.333)", (step_rises - 1) * 16000000.0 / (double)(step_last - step_first));
    printf("\n");
    avr_cycle_count_t tc = avr->cycle;
    turn(1);                                          // -> 2 rpm
    step_rises = 0; run_for(3000);
    printf("after click to 2 rpm: first new pulse %.0f ms after the click; %ld rises", (step_first - tc) / 16000.0, step_rises);
    if (step_rises > 1) printf(" -> %.3f Hz (expect 6.667)", (step_rises - 1) * 16000000.0 / (double)(step_last - step_first));
    printf("\n");
    snprintf(dn, sizeof dn, "%s_run", scen); dump_screen(dn);
  }
  if (!strcmp(scen, "coilA") || !strcmp(scen, "nomotor")) {
    press(100); run_for(300);
    press(800); run_for(300); turn(11); press(100); run_for(3200);
    snprintf(dn, sizeof dn, "%s_coil", scen); dump_screen(dn);
  }
  if (!strcmp(scen, "nooled")) {
    led_n = 0; run_for(8000);
    printf("LED rising edges after the test (ms):");
    for (int i = 0; i < led_n; i++) printf(" %.0f", led_t[i] / 16000.0);
    printf("\n");
  }
  if (!strcmp(scen, "vmdrop")) {                      // brick sags while running
    press(100); run_for(300); press(100); run_for(1000);
    printf("before drop: EN %d VACTUAL %d (writes %d)\n", out_level(DDRD_A, 7), vact_log_n ? vact_log_v[vact_log_n - 1] : 0, vact_log_n);
    snprintf(dn, sizeof dn, "%s_running", scen); dump_screen(dn);
    set_vm(7000); run_for(500);
    printf("vm drop: EN %d VACTUAL %d\n", out_level(DDRD_A, 7), vact_log_n ? vact_log_v[vact_log_n - 1] : 0);
    snprintf(dn, sizeof dn, "%s_fault", scen); dump_screen(dn);
  }
  dbg_sample_to(avr->cycle);                          // flush the last byte of the debug log
  printf("stack: lowest SP 0x%04X -> %d bytes of stack used at worst (RAMEND 0x08FF)\n", sp_min, 0x8FF - sp_min);
  printf("final: tmc reads %d writes %d crcerr %d | debug edges %ld, worst edge offset %.1f cycles (bit = %.1f)\n",
         nreads, nwrites, crcerr, edges_n, edge_err_max, bit_cyc);
  fclose(dbgout);
  return 0;
}
