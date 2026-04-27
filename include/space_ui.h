#pragma once
#include <M5Stack.h>

// ── Space theme palette (RGB565) ───────────────────────────────────────────
#define SP_NEON_C   ((uint16_t)0x07FF)  // cyan
#define SP_NEON_M   ((uint16_t)0xF81F)  // magenta
#define SP_NEON_G   ((uint16_t)0x07E0)  // green
#define SP_NEON_Y   ((uint16_t)0xFFE0)  // gold
#define SP_NEON_R   ((uint16_t)0xF800)  // red
#define SP_HDR_BG   ((uint16_t)0x000C)  // deep navy
#define SP_GRID     ((uint16_t)0x2104)  // dim purple grid
#define SP_MSG_BG   ((uint16_t)0x0802)  // dark nebula
#define SP_DIM_C    ((uint16_t)0x0294)  // dim cyan text
#define SP_STAR_DIM ((uint16_t)0x4208)  // dim star
#define SP_STAR_BRT ((uint16_t)0xCE59)  // bright star

// ── Starfield (deterministic via coordinate hash) ─────────────────────────
inline void spDrawStarfield(int x, int y, int w, int h) {
    for (int px = x; px < x + w; px++) {
        for (int py = y; py < y + h; py++) {
            uint32_t v = ((uint32_t)px * 2654435761u) ^ ((uint32_t)py * 2246822519u);
            if ((v & 0xFF) < 8) {
                uint16_t c = ((v >> 8) & 0xFF) < 120 ? SP_STAR_DIM : SP_STAR_BRT;
                M5.Lcd.drawPixel(px, py, c);
            }
        }
    }
}

// ── Corner-accent border ──────────────────────────────────────────────────
inline void spCornerFrame(int x, int y, int w, int h, uint16_t col) {
    const int S = 8;
    M5.Lcd.drawRect(x, y, w, h, SP_GRID);
    M5.Lcd.drawFastHLine(x,     y,     S, col); M5.Lcd.drawFastVLine(x,     y,     S, col);
    M5.Lcd.drawFastHLine(x+w-S, y,     S, col); M5.Lcd.drawFastVLine(x+w-1, y,     S, col);
    M5.Lcd.drawFastHLine(x,     y+h-1, S, col); M5.Lcd.drawFastVLine(x,     y+h-S, S, col);
    M5.Lcd.drawFastHLine(x+w-S, y+h-1, S, col); M5.Lcd.drawFastVLine(x+w-1, y+h-S, S, col);
}

// ── Neon energy bar ───────────────────────────────────────────────────────
inline void spNeonBar(int x, int y, int bw, int bh, uint8_t val, uint16_t col) {
    int filled = map(val, 0, 100, 0, bw);
    M5.Lcd.fillRect(x, y, bw, bh, SP_GRID);
    if (filled > 0) {
        M5.Lcd.fillRect(x, y, filled, bh, col);
        M5.Lcd.drawFastVLine(x + filled - 1, y, bh, TFT_WHITE); // glow tip
    }
}

inline uint16_t spStatColor(uint8_t val) {
    return val > 66 ? SP_NEON_C : val > 33 ? SP_NEON_Y : SP_NEON_R;
}
