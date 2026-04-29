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
