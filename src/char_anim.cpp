#include "char_anim.h"
#include "sprite_stages.h"
#include "space_ui.h"
#include <M5Stack.h>
#include <math.h>

static const float BOB_AMP  = 3.0f;
static const float BOB_FREQ = 0.0015f;

static uint16_t buf[SPRITE_W * SPRITE_H];
static int      currentStage = -1;
static int      lastBobY     = -999;
static int      lastCX       = -1;
static int      lastCY       = -1;

static int stageForAge(uint8_t age) {
    if (age < 20) return 0;
    if (age < 40) return 1;
    if (age < 60) return 2;
    if (age < 80) return 3;
    return 4;
}

static void loadStage(int stage) {
    memcpy(buf, SPRITES[stage], SPRITE_W * SPRITE_H * sizeof(uint16_t));
    currentStage = stage;
}

static void drawChar(int cx, int cy, int bobY) {
    M5.Lcd.pushImage(cx - SPRITE_W / 2, cy - SPRITE_H / 2 + bobY,
                     SPRITE_W, SPRITE_H, buf, (uint16_t)0x0000);
}

static void eraseChar(int cx, int cy, int bobY) {
    spDrawBackgroundRect(cx - SPRITE_W / 2, cy - SPRITE_H / 2 + bobY, SPRITE_W, SPRITE_H);
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
