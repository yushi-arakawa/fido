import sys, os
sys.stdout.reconfigure(encoding='utf-8')
from PIL import Image

SIZE = 64
SRC  = r"c:\Users\yushi\Desktop\fido\fido\src\char"
OUT  = r"c:\Users\yushi\Desktop\fido\fido\include\sprite_stages.h"

stages = [
    ("卵",    "SPRITE_EGG"),
    ("幼少期","SPRITE_CHILD"),
    ("思春期","SPRITE_TEEN"),
    ("青年期","SPRITE_YOUNG"),
    ("老年期","SPRITE_OLD"),
]

BG_TOL = 35

def bg_color(img):
    w, h = img.size
    corners = [img.getpixel((0, 0)), img.getpixel((w-1, 0)),
               img.getpixel((0, h-1)), img.getpixel((w-1, h-1))]
    return corners[0]

def dist(a, b):
    return max(abs(int(a[i]) - int(b[i])) for i in range(3))

def to_rgb565_swapped(r, g, b):
    v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return ((v >> 8) & 0xFF) | ((v & 0xFF) << 8)

try:
    resample = Image.Resampling.LANCZOS
except AttributeError:
    resample = Image.LANCZOS

arrays = {}
for fname, varname in stages:
    path = os.path.join(SRC, fname + ".jpg")
    img  = Image.open(path).convert("RGB").resize((SIZE, SIZE), resample)
    bg   = bg_color(img)
    pixels = []
    for y in range(SIZE):
        for x in range(SIZE):
            px = img.getpixel((x, y))
            if dist(px[:3], bg[:3]) <= BG_TOL:
                pixels.append(0x0000)
            else:
                v = to_rgb565_swapped(*px[:3])
                pixels.append(v if v != 0 else 0x0800)
    arrays[varname] = pixels
    vis = sum(1 for p in pixels if p != 0)
    print(f"  {fname}: {vis}/{SIZE*SIZE} visible  bg={bg[:3]}")

with open(OUT, 'w', encoding='utf-8') as f:
    f.write("#pragma once\n#include <Arduino.h>\n\n")
    f.write(f"static const uint16_t SPRITE_W    = {SIZE};\n")
    f.write(f"static const uint16_t SPRITE_H    = {SIZE};\n")
    f.write(f"static const int      STAGE_COUNT = 5;\n\n")
    for varname, pixels in arrays.items():
        f.write(f"static const uint16_t {varname}[{SIZE*SIZE}] = {{\n")
        for i in range(0, len(pixels), 16):
            f.write("    " + ", ".join(f"0x{p:04X}" for p in pixels[i:i+16]) + ",\n")
        f.write("};\n\n")
    f.write("static const uint16_t* const SPRITES[STAGE_COUNT] = {\n")
    for _, vn in stages:
        f.write(f"    {vn},\n")
    f.write("};\n")

print(f"Written: {OUT}")
