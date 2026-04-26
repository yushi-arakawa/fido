#include "minigame.h"
#include <M5Stack.h>

static const int ROUNDS = 3;

static int coinsForReaction(uint32_t ms) {
    if (ms < 300)  return 5;
    if (ms < 500)  return 4;
    if (ms < 800)  return 3;
    if (ms < 1200) return 2;
    if (ms < 2000) return 1;
    return 0;
}

uint16_t runMiniGame() {
    uint16_t total = 0;

    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(60, 90);
    M5.Lcd.print("Reaction Game!");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(70, 125);
    M5.Lcd.print("Press B when screen flashes");
    delay(2000);

    for (int r = 0; r < ROUNDS; r++) {
        // "Get ready" screen
        M5.Lcd.fillScreen(TFT_BLACK);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Lcd.setCursor(90, 80);
        M5.Lcd.printf("Round %d/%d", r + 1, ROUNDS);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(100, 115);
        M5.Lcd.print("Get ready...");

        // Random wait 1-3 s
        delay(1000 + random(2000));

        // Flash: GO!
        M5.Lcd.fillScreen(TFT_GREEN);
        M5.Lcd.setTextSize(3);
        M5.Lcd.setTextColor(TFT_BLACK, TFT_GREEN);
        M5.Lcd.setCursor(100, 95);
        M5.Lcd.print("PRESS B!");

        uint32_t t0 = millis();
        bool pressed = false;
        while (millis() - t0 < 3000) {
            M5.update();
            if (M5.BtnB.wasPressed()) { pressed = true; break; }
            delay(10);
        }

        uint32_t reactionMs = millis() - t0;
        int earned = pressed ? coinsForReaction(reactionMs) : 0;
        total += earned;

        // Result
        M5.Lcd.fillScreen(TFT_BLACK);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Lcd.setCursor(50, 80);
        if (pressed) {
            M5.Lcd.printf("%dms", reactionMs);
            M5.Lcd.setCursor(50, 110);
            M5.Lcd.printf("+%d coins!", earned);
        } else {
            M5.Lcd.print("Too slow!");
        }
        delay(1500);
    }

    // Final result
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Lcd.setCursor(60, 95);
    M5.Lcd.printf("Total: +%d coins!", total);
    delay(2500);

    return total;
}
