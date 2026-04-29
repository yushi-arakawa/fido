#pragma once
#include <M5Stack.h>

// ── Monochrome space palette (pure greyscale) ──────────────────────────────
#define SM_BG       ((uint16_t)0x0000)  // black
#define SM_HDR      ((uint16_t)0x0841)  // ~3% grey  (header strip)
#define SM_SEL      ((uint16_t)0x2945)  // ~16% grey (selection highlight)
#define SM_DIV      ((uint16_t)0x2104)  // ~12% grey (divider lines)
#define SM_DIM      ((uint16_t)0x4208)  // ~25% grey (dim / empty bar)
#define SM_GREY     ((uint16_t)0x7BEF)  // ~50% grey (body text)
#define SM_LIGHT    ((uint16_t)0xBDF7)  // ~75% grey (normal text)
#define SM_WHITE    ((uint16_t)0xFFFF)  // white      (titles / emphasis)
#define SM_BORDER   ((uint16_t)0x4208)  // border colour

// ── Sky gradient background (top=black → bottom=dark navy) ────────────────
// Call once when opening a full-screen view; do NOT call on every redraw.
inline void spDrawBackground() {
    M5.Lcd.fillRect(0,   0, 320,  80, 0x0000);
    M5.Lcd.fillRect(0,  80, 320,  50, 0x0021);
    M5.Lcd.fillRect(0, 130, 320,  50, 0x0062);
    M5.Lcd.fillRect(0, 180, 320,  30, 0x0884);
    M5.Lcd.fillRect(0, 210, 320,  30, 0x08C5);
}

// ── Starfield (low density, monochrome, deterministic) ────────────────────
// Only call once on a static screen; individual drawPixel is slow/noisy.
inline void spDrawStarfield(int x, int y, int w, int h) {
    for (int px = x; px < x + w; px++) {
        for (int py = y; py < y + h; py++) {
            uint32_t v = ((uint32_t)px * 2654435761u) ^ ((uint32_t)py * 2246822519u);
            if ((v & 0x1FF) < 3) {          // ~0.6% density → ~460 stars full-screen
                uint8_t br = (v >> 9) & 0xFF;
                uint16_t c = br < 80  ? SM_DIM   :
                             br < 160 ? SM_GREY  :
                             br < 230 ? SM_LIGHT : SM_WHITE;
                M5.Lcd.drawPixel(px, py, c);
            }
        }
    }
}

// ── Corner-accent frame ───────────────────────────────────────────────────
inline void spCornerFrame(int x, int y, int w, int h, uint16_t col = SM_BORDER) {
    const int S = 6;
    M5.Lcd.drawRect(x, y, w, h, SM_DIV);
    M5.Lcd.drawFastHLine(x,     y,     S, col); M5.Lcd.drawFastVLine(x,     y,     S, col);
    M5.Lcd.drawFastHLine(x+w-S, y,     S, col); M5.Lcd.drawFastVLine(x+w-1, y,     S, col);
    M5.Lcd.drawFastHLine(x,     y+h-1, S, col); M5.Lcd.drawFastVLine(x,     y+h-S, S, col);
    M5.Lcd.drawFastHLine(x+w-S, y+h-1, S, col); M5.Lcd.drawFastVLine(x+w-1, y+h-S, S, col);
}

// ── Background restore for a sub-rect (used by char erase in Main mode) ──
inline void spDrawBackgroundRect(int x, int y, int w, int h) {
    for (int row = y; row < y + h; row++) {
        uint16_t c = row <  80 ? (uint16_t)0x0000 :
                     row < 130 ? (uint16_t)0x0021 :
                     row < 180 ? (uint16_t)0x0062 :
                     row < 210 ? (uint16_t)0x0884 : (uint16_t)0x08C5;
        M5.Lcd.drawFastHLine(x, row, w, c);
    }
    spDrawStarfield(x, y, w, h);
}

// ── Monochrome bar ────────────────────────────────────────────────────────
inline void spBar(int x, int y, int bw, int bh, uint8_t val) {
    int filled = map(val, 0, 100, 0, bw);
    M5.Lcd.fillRect(x, y, bw, bh, SM_DIV);
    if (filled > 0) {
        M5.Lcd.fillRect(x, y, filled, bh, SM_WHITE);
        if (filled > 1)
            M5.Lcd.drawFastVLine(x + filled - 1, y, bh, SM_LIGHT); // soft edge
    }
}
