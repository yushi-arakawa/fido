import sys, os, struct
sys.stdout.reconfigure(encoding='utf-8')
from PIL import Image

SIZE   = 128  # native sprite size — no firmware scaling needed
SRC    = r"c:\Users\yushi\Desktop\fido\fido\src\char"
SD_DIR = r"c:\Users\yushi\Desktop\fido\fido\sdcard\char"  # copy to SD root /char/
H_OUT  = r"c:\Users\yushi\Desktop\fido\fido\include\sprite_stages.h"

stages = [
    ("卵",    "egg"),
    ("幼少期","child"),
    ("思春期","teen"),
    ("青年期","young"),
    ("老年期","elder"),
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

os.makedirs(SD_DIR, exist_ok=True)

for fname, varname in stages:
    path = os.path.join(SRC, fname + ".jpg")
    img  = Image.open(path).convert("RGB").resize((SIZE, SIZE), resample)
    bg   = bg_color(img)
    out  = os.path.join(SD_DIR, varname + ".bin")
    vis  = 0
    with open(out, 'wb') as f:
        for y in range(SIZE):
            for x in range(SIZE):
                px = img.getpixel((x, y))
                if dist(px[:3], bg[:3]) <= BG_TOL:
                    f.write(struct.pack('<H', 0x0000))
                else:
                    v = to_rgb565_swapped(*px[:3])
                    val = v if v != 0 else 0x0800
                    f.write(struct.pack('<H', val))
                    vis += 1
    print(f"  {fname}: {vis}/{SIZE*SIZE} visible  bg={bg[:3]}  -> {varname}.bin")

# Minimal header — pixel data lives on SD, not in flash
with open(H_OUT, 'w', encoding='utf-8') as f:
    f.write("#pragma once\n#include <Arduino.h>\n\n")
    f.write(f"static const uint16_t SPRITE_W    = {SIZE};\n")
    f.write(f"static const uint16_t SPRITE_H    = {SIZE};\n")
    f.write(f"static const int      STAGE_COUNT = 5;\n")

print(f"\nHeader:   {H_OUT}")
print(f"SD files: {SD_DIR}")
print(f"\n-> Copy sdcard/char/ to the root of your microSD card.")
