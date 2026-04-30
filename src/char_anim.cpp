#include "char_anim.h"
#include "sprite_stages.h"
#include "space_ui.h"
#include <M5Stack.h>
#include <math.h>

static const float BOB_AMP  = 3.0f;
static const float BOB_FREQ = 0.0015f;
static const int   SCALE    = 2;          // 64x64 -> 128x128 at draw time
static const int   DW       = SPRITE_W * SCALE;
static const int   DH       = SPRITE_H * SCALE;

static uint16_t buf[SPRITE_W * SPRITE_H]; // 64x64 source in DRAM
static uint16_t scaledBuf[DW * DH];       // 128x128 scaled in DRAM
static int      currentStage = -1;
static int      lastBobY     = -999;
static int      lastCX       = -1;
static int      lastCY       = -1;

static int stageForAge(uint8_t age) {
    if (age < 4)  return 0; // Egg: ~2 min (4 ticks x 30s)
    if (age < 40) return 1;
    if (age < 60) return 2;
    if (age < 80) return 3;
    return 4;
}

static void loadStage(int stage) {
    memcpy(buf, SPRITES[stage], SPRITE_W * SPRITE_H * sizeof(uint16_t));
    // Pre-scale 64x64 -> 128x128 (nearest-neighbour, done once per stage change)
    for (int y = 0; y < SPRITE_H; y++) {
        for (int x = 0; x < SPRITE_W; x++) {
            uint16_t px = buf[y * SPRITE_W + x];
            int dx = x * SCALE, dy = y * SCALE;
            scaledBuf[ dy      * DW + dx    ] = px;
            scaledBuf[ dy      * DW + dx + 1] = px;
            scaledBuf[(dy + 1) * DW + dx    ] = px;
            scaledBuf[(dy + 1) * DW + dx + 1] = px;
        }
    }
    currentStage = stage;
}

static void drawChar(int cx, int cy, int bobY) {
    M5.Lcd.pushImage(cx - DW / 2, cy - DH / 2 + bobY,
                     DW, DH, scaledBuf, (uint16_t)0x0000);
}

static void eraseChar(int cx, int cy, int bobY) {
    spDrawBackgroundRect(cx - DW / 2, cy - DH / 2 + bobY, DW, DH);
}

void charAnimRedraw() { lastBobY = -999; }

// ─── Evolution animation ──────────────────────────────────────────────────

static void playEvolveAnim(int cx, int cy) {
    // Phase 1: three full-area pulses with rising tones (C4→E4→G4)
    const uint16_t pnotes[] = {262, 330, 392};
    for (int i = 0; i < 3; i++) {
        M5.Lcd.fillRect(0, 0, 320, 155, SM_WHITE);
        M5.Speaker.tone(pnotes[i], 100);
        delay(130);
        spDrawBackgroundRect(0, 0, 320, 155);
        delay(70);
    }

    // Phase 2: sparkles spiral outward and accumulate
    int pMinX = cx, pMinY = cy, pMaxX = cx, pMaxY = cy;
    for (int frame = 0; frame < 12; frame++) {
        float r  = 30.0f + frame * 9.0f;
        float ao = frame * 12.0f * (PI / 180.0f);
        uint16_t col = (frame & 1) ? SM_WHITE : (uint16_t)0xFFE0;
        for (int p = 0; p < 8; p++) {
            float angle = p * (PI / 4.0f) + ao;
            int px = (int)(cx + r * cosf(angle));
            int py = (int)(cy + r * sinf(angle));
            if (px >= 2 && px < 318 && py >= 2 && py < 153) {
                M5.Lcd.fillRect(px - 1, py - 1, 3, 3, col);
                if (px - 2 < pMinX) pMinX = px - 2;
                if (py - 2 < pMinY) pMinY = py - 2;
                if (px + 3 > pMaxX) pMaxX = px + 3;
                if (py + 3 > pMaxY) pMaxY = py + 3;
            }
        }
        delay(35);
    }
    if (pMaxX > pMinX) spDrawBackgroundRect(pMinX, pMinY, pMaxX - pMinX, pMaxY - pMinY);

    // Phase 3: final full-area flash + victory tone (C5)
    M5.Lcd.fillRect(0, 0, 320, 155, SM_WHITE);
    M5.Speaker.tone(523, 350);
    delay(350);
    spDrawBackgroundRect(0, 0, 320, 155);
}

// ─── Startup animation ────────────────────────────────────────────────────

void charAnimPlayStartup() {
    spDrawBackground();
    spDrawStarfield(0, 0, 320, 240);

    // "FIDO" letter by letter with ascending arpeggio (C4 E4 G4 B4)
    const uint16_t lnotes[] = {262, 330, 392, 494};
    const char letters[] = "FIDO";
    M5.Lcd.setTextSize(6);
    M5.Lcd.setTextColor(SM_WHITE, SM_BG);
    const int TX = (320 - 4 * 36) / 2;  // 88 — center 4 chars of width 36
    const int TY = 82;
    for (int i = 0; i < 4; i++) {
        M5.Lcd.setCursor(TX + i * 36, TY);
        M5.Lcd.print(letters[i]);
        M5.Speaker.tone(lnotes[i], 80);
        delay(200);
    }

    // Subtitle
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(78, TY + 52);
    M5.Lcd.print("   your space companion");
    delay(400);

    delay(600);
    // fullRedraw() will overwrite the screen; no explicit clear needed
}

void charAnimUpdate(uint8_t age, int cx, int cy) {
    int stage = stageForAge(age);

    if ((cx != lastCX || cy != lastCY) && lastBobY != -999 && lastCX != -1) {
        eraseChar(lastCX, lastCY, lastBobY);
        lastBobY = -999;
    }
    lastCX = cx;
    lastCY = cy;

    if (stage != currentStage) {
        if (lastBobY != -999) eraseChar(cx, cy, lastBobY);
        if (currentStage >= 0) playEvolveAnim(cx, cy);  // skip on first load
        loadStage(stage);
        lastBobY = -999;
    }

    int bobY = (int)(sinf(millis() * BOB_FREQ) * BOB_AMP);
    if (bobY != lastBobY) {
        if (lastBobY != -999) eraseChar(cx, cy, lastBobY);
        drawChar(cx, cy, bobY);
        lastBobY = bobY;
    }
}
