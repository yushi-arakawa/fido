#include "char_anim.h"
#include "sprite_data.h"
#include <M5Stack.h>
#include <math.h>

static const int   CX       = 245;
static const int   CY       = 75;
static const float BOB_AMP  = 4.0f;
static const float BOB_FREQ = 0.0015f;

// Copy from flash to DRAM once so pushImage can access via SPI
static uint16_t buf[SPRITE_W * SPRITE_H];
static bool     bufReady = false;
static int      lastBobY = -999;

static void drawChar(int bobY) {
    M5.Lcd.pushImage(CX - SPRITE_W / 2, CY - SPRITE_H / 2 + bobY,
                     SPRITE_W, SPRITE_H, buf);
}

static void eraseChar(int bobY) {
    M5.Lcd.fillRect(CX - SPRITE_W / 2, CY - SPRITE_H / 2 + bobY,
                    SPRITE_W, SPRITE_H, TFT_BLACK);
}

void charAnimRedraw() {
    lastBobY = -999;
}

void charAnimUpdate() {
    if (!bufReady) {
        memcpy(buf, SPRITE_DATA, sizeof(buf));
        bufReady = true;
    }

    int bobY = (int)(sinf(millis() * BOB_FREQ) * BOB_AMP);

    if (bobY != lastBobY) {
        if (lastBobY != -999) eraseChar(lastBobY);
        drawChar(bobY);
        lastBobY = bobY;
    }
}
