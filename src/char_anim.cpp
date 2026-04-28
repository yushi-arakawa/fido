#include "char_anim.h"
#include "sprite_stages.h"
#include <M5Stack.h>
#include <math.h>

static const int   CX       = 242;
static const int   CY       = 77;
static const float BOB_AMP  = 3.0f;
static const float BOB_FREQ = 0.0015f;

static uint16_t buf[SPRITE_W * SPRITE_H]; // DRAM buffer for SPI DMA
static int      currentStage = -1;
static int      lastBobY     = -999;

static int stageForAge(uint8_t age) {
    if (age < 20) return 0; // egg    (~0-9 min)
    if (age < 40) return 1; // child  (~10-19 min)
    if (age < 60) return 2; // teen   (~20-29 min)
    if (age < 80) return 3; // young  (~30-39 min)
    return 4;               // elder  (~40+ min)
}

static void loadStage(int stage) {
    memcpy(buf, SPRITES[stage], SPRITE_W * SPRITE_H * sizeof(uint16_t));
    currentStage = stage;
}

static void drawChar(int bobY) {
    M5.Lcd.pushImage(CX - SPRITE_W / 2, CY - SPRITE_H / 2 + bobY,
                     SPRITE_W, SPRITE_H, buf, (uint16_t)0x0000); // 0x0000=transparent
}

static void eraseChar(int bobY) {
    M5.Lcd.fillRect(CX - SPRITE_W / 2, CY - SPRITE_H / 2 + bobY,
                    SPRITE_W, SPRITE_H, TFT_BLACK);
}

void charAnimRedraw() {
    lastBobY = -999;
}

void charAnimUpdate(uint8_t age) {
    int stage = stageForAge(age);

    if (stage != currentStage) {
        if (lastBobY != -999) eraseChar(lastBobY);
        loadStage(stage);
        lastBobY = -999; // force redraw with new sprite
    }

    int bobY = (int)(sinf(millis() * BOB_FREQ) * BOB_AMP);

    if (bobY != lastBobY) {
        if (lastBobY != -999) eraseChar(lastBobY);
        drawChar(bobY);
        lastBobY = bobY;
    }
}
