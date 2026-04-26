#include "shop.h"
#include <M5Stack.h>

static const uint16_t BORDER = 0x05BF;

static void drawShop(int sel, const Inventory& inv) {
    M5.Lcd.fillScreen(TFT_BLACK);

    // Header
    M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 8);
    M5.Lcd.print("SHOP");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(180, 12);
    M5.Lcd.printf("Coins: %d", inv.coins);

    M5.Lcd.drawFastHLine(0, 30, 320, BORDER);

    // Item list
    for (int i = 0; i < ITEM_COUNT; i++) {
        int y = 38 + i * 38;
        bool selected = (i == sel);
        bool owned    = inv.owned[i];

        M5.Lcd.fillRect(2, y - 2, 316, 34, selected ? TFT_DARKGREY : TFT_BLACK);
        M5.Lcd.drawRect(2, y - 2, 316, 34, selected ? BORDER : TFT_DARKGREY);

        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(owned ? TFT_DARKGREY : (selected ? TFT_YELLOW : TFT_WHITE),
                            selected ? TFT_DARKGREY : TFT_BLACK);
        M5.Lcd.setCursor(12, y + 2);
        M5.Lcd.print(ITEM_DEFS[i].name);

        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(TFT_CYAN, selected ? TFT_DARKGREY : TFT_BLACK);
        M5.Lcd.setCursor(12, y + 22);
        M5.Lcd.print(ITEM_DEFS[i].desc);

        M5.Lcd.setTextColor(owned ? TFT_DARKGREY : TFT_GREEN,
                            selected ? TFT_DARKGREY : TFT_BLACK);
        M5.Lcd.setCursor(230, y + 8);
        if (owned) {
            M5.Lcd.print("Owned");
        } else {
            M5.Lcd.printf("%d coins", ITEM_DEFS[i].cost);
        }
    }

    // Footer
    M5.Lcd.drawFastHLine(0, 210, 320, BORDER);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(10,  218); M5.Lcd.print("[Move]");
    M5.Lcd.setCursor(130, 218); M5.Lcd.print("[Buy]");
    M5.Lcd.setCursor(244, 218); M5.Lcd.print("[Back]");
}

void runShop(Pet& pet, Inventory& inv) {
    int  sel     = 0;
    bool running = true;
    drawShop(sel, inv);

    while (running) {
        M5.update();

        if (M5.BtnA.wasPressed()) {
            sel = (sel + 1) % ITEM_COUNT;
            drawShop(sel, inv);
        }

        if (M5.BtnB.wasPressed()) {
            if (!inv.owned[sel] && inv.coins >= ITEM_DEFS[sel].cost) {
                inv.coins -= ITEM_DEFS[sel].cost;
                inv.owned[sel] = true;
                applyItem(pet, sel);
                saveAll(pet, inv);
                drawShop(sel, inv);

                // Flash "Purchased!"
                M5.Lcd.fillRect(90, 95, 140, 30, TFT_GREEN);
                M5.Lcd.setTextColor(TFT_BLACK, TFT_GREEN);
                M5.Lcd.setTextSize(2);
                M5.Lcd.setCursor(100, 102);
                M5.Lcd.print("Bought!");
                delay(800);
                drawShop(sel, inv);
            }
        }

        if (M5.BtnC.wasPressed()) running = false;
        delay(16);
    }
}
