#!/usr/bin/env python3
"""Labeled reference sheet of simulated OLED screens (docs/images/sim-screens.png).

usage: python3 tools/sim/sheet.py [out_dir] [output.png]
       defaults: build/sim/out  docs/images/sim-screens.png
Run tools/sim/run_all.sh first so the .pgm screen dumps exist. Needs Pillow.
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "sim", "out")
dest = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "docs", "images", "sim-screens.png")

items = [
    ("happy_post", "Power-up self test, all pass"),
    ("happy_main_stopped", "Main screen, stopped (knob sets speed)"),
    ("happy_main_running", "Running: actual rpm, current, load bar"),
    ("happy_menu_top", "Hold knob: menu"),
    ("happy_coil", "Menu > Coil check, motor good"),
    ("nomotor_coil", "Coil check, nothing plugged in"),
    ("addr2_post", "Fault example: MS2 not grounded"),
    ("notmc_post", "Fault example: driver not answering"),
    ("enbroken_post", "Fault example: EN trace open"),
    ("novm_post", "Board on Uno power only (no brick)"),
    ("vmdrop_fault", "Brick sagged while running: stopped"),
    ("happy_regs", "Menu > Driver regs (live)"),
]
S = 3
try:
    font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 15)
except Exception:
    font = ImageFont.load_default()
tiles = []
for name, label in items:
    im = Image.open(os.path.join(out_dir, f"{name}.pgm")).convert("L").resize((128 * S, 64 * S), Image.NEAREST)
    rgb = Image.new("RGB", im.size, (6, 10, 18))
    px, dst = im.load(), rgb.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            if px[x, y]:
                dst[x, y] = (140, 215, 255)
    tile = Image.new("RGB", (128 * S + 20, 64 * S + 44), (34, 36, 40))
    tile.paste(rgb, (10, 10))
    ImageDraw.Draw(tile).text((10, 64 * S + 16), label, fill=(225, 228, 232), font=font)
    tiles.append(tile)
w, h = tiles[0].size
cols = 2
rows = (len(tiles) + cols - 1) // cols
sheet = Image.new("RGB", (w * cols, h * rows), (28, 30, 34))
for i, t in enumerate(tiles):
    sheet.paste(t, ((i % cols) * w, (i // cols) * h))
sheet.save(dest)
print(dest, sheet.size)
