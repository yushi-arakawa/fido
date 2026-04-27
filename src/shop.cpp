#include "shop.h"
#include "space_ui.h"
#include <M5Stack.h>

static const int VISIBLE = 5;

static void drawShop(int sel, const Inventory& inv) {
    M5.Lcd.fillScreen(TFT_BLACK);
    spDrawStarfield(0, 0, 320, 240);

    // Header
    M5.Lcd.fillRect(0, 0, 320, 28, SP_HDR_BG);
    M5.Lcd.setTextColor(SP_NEON_Y, SP_HDR_BG);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 6);
    M5.Lcd.print("SPACE SHOP");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SP_NEON_C, SP_HDR_BG);
    M5.Lcd.setCursor(220, 11);
    M5.Lcd.printf("$%d", inv.coins);
    M5.Lcd.drawFastHLine(0, 28, 320, SP_NEON_C);

    // Scrolling item list
    int start = max(0, min(sel - VISIBLE / 2, ITEM_COUNT - VISIBLE));

    for (int i = 0; i < VISIBLE && (start + i) < ITEM_COUNT; i++) {
        int idx      = start + i;
        int y        = 32 + i * 38;
        bool selected = (idx == sel);
        bool owned    = inv.owned[idx];

        uint16_t rowBg = selected ? SP_HDR_BG : TFT_BLACK;
        M5.Lcd.fillRect(2, y, 316, 35, rowBg);
        spCornerFrame(2, y, 316, 35, selected ? SP_NEON_M : SP_GRID);

        if (selected) {
            M5.Lcd.fillRect(2, y, 3, 35, SP_NEON_M); // accent bar
        }

        // Item name
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(
            owned ? SP_GRID : (selected ? SP_NEON_Y : TFT_WHITE),
            rowBg);
        M5.Lcd.setCursor(12, y + 3);
        M5.Lcd.print(ITEM_DEFS[idx].name);

        // Description
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(selected ? SP_NEON_C : SP_DIM_C, rowBg);
        M5.Lcd.setCursor(12, y + 24);
        M5.Lcd.print(ITEM_DEFS[idx].desc);

        // Price / owned
        M5.Lcd.setCursor(238, y + 10);
        if (owned) {
            M5.Lcd.setTextColor(SP_GRID, rowBg);
            M5.Lcd.print("Owned");
        } else {
            M5.Lcd.setTextColor(selected ? SP_NEON_Y : SP_NEON_G, rowBg);
            M5.Lcd.printf("$%d", ITEM_DEFS[idx].cost);
        }
    }

    // Scroll indicator
    if (ITEM_COUNT > VISIBLE) {
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(SP_GRID, TFT_BLACK);
        M5.Lcd.setCursor(292, 120);
        M5.Lcd.printf("%d/%d", sel + 1, ITEM_COUNT);
    }

    // Footer
    spCornerFrame(0, 0, 320, 240, SP_NEON_C);
    M5.Lcd.drawFastHLine(0, 225, 320, SP_NEON_M);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SP_NEON_G,  TFT_BLACK); M5.Lcd.setCursor(10,  231); M5.Lcd.print("[A:MOVE]");
    M5.Lcd.setTextColor(SP_NEON_Y,  TFT_BLACK); M5.Lcd.setCursor(115, 231); M5.Lcd.print("[B:BUY]");
    M5.Lcd.setTextColor(SP_NEON_M,  TFT_BLACK); M5.Lcd.setCursor(225, 231); M5.Lcd.print("[C:BACK]");
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

                // Flash "ACQUIRED!" in neon
                M5.Lcd.fillRect(80, 95, 160, 36, SP_NEON_G);
                M5.Lcd.setTextColor(TFT_BLACK, SP_NEON_G);
                M5.Lcd.setTextSize(2);
                M5.Lcd.setCursor(90, 107);
                M5.Lcd.print("ACQUIRED!");
                delay(800);
                drawShop(sel, inv);
            }
        }

        if (M5.BtnC.wasPressed()) running = false;
        delay(16);
    }
}
