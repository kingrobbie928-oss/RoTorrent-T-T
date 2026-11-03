#!/usr/bin/env python3
"""Builds dist/windows/rotorrent.ico: the RO mark on a rounded dark tile (same look as the tray icon).
Usage: python3 tools/make_ico.py    (needs Pillow). The generated .ico is committed."""
from PIL import Image, ImageDraw
import pathlib
root = pathlib.Path(__file__).resolve().parent.parent
logo = Image.open(root / "src/resources/ro/assets/logo_dark.png").convert("RGBA")
SIZES = [16, 24, 32, 48, 64, 128, 256]
TILE = (23, 23, 27, 255)          # theme surface (#17171b)
SS = 4                            # supersample for a smooth rounded edge

def tile(px):
    big = px * SS
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    ImageDraw.Draw(img).rounded_rectangle([0, 0, big - 1, big - 1], radius=int(big * 0.22), fill=TILE)
    inner = int(big * 0.70)                       # logo width inside the tile
    h = round(inner * logo.height / logo.width)
    m = logo.resize((inner, h), Image.LANCZOS)
    img.alpha_composite(m, ((big - inner) // 2, (big - h) // 2))
    return img.resize((px, px), Image.LANCZOS)

imgs = [tile(s) for s in SIZES]
out = root / "dist/windows/rotorrent.ico"
imgs[-1].save(out, format="ICO", sizes=[(s, s) for s in SIZES], append_images=imgs[:-1])
print("wrote", out, out.stat().st_size, "bytes")
