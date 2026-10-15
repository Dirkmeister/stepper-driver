#!/usr/bin/env python3
"""Contact sheet of any simulated OLED dumps (unlabeled, file names as captions).

usage: python3 tools/sim/render.py out.png build/sim/out/happy_*.pgm
       python3 tools/sim/render.py out.png            (every .pgm in build/sim/out)
Needs Pillow.
"""
import glob
import os
import sys
from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
out = sys.argv[1]
files = sys.argv[2:] or sorted(glob.glob(os.path.join(ROOT, "build", "sim", "out", "*.pgm")))
S = 3
tiles = []
for f in files:
    im = Image.open(f).convert("L").resize((128 * S, 64 * S), Image.NEAREST)
    rgb = Image.new("RGB", im.size, (8, 12, 20))
    px, dst = im.load(), rgb.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            if px[x, y]:
                dst[x, y] = (150, 220, 255)
    tile = Image.new("RGB", (128 * S + 16, 64 * S + 34), (40, 40, 40))
    tile.paste(rgb, (8, 8))
    ImageDraw.Draw(tile).text((8, 64 * S + 14), os.path.basename(f).replace(".pgm", ""), fill=(230, 230, 230))
    tiles.append(tile)
cols = 2
w, h = tiles[0].size
rows = (len(tiles) + cols - 1) // cols
sheet = Image.new("RGB", (w * cols, h * rows), (30, 30, 30))
for i, t in enumerate(tiles):
    sheet.paste(t, ((i % cols) * w, (i // cols) * h))
sheet.save(out)
print(out, sheet.size)
