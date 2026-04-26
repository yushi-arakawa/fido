#include "status_report.h"
#include <M5Stack.h>

static const uint16_t BORDER = 0x05BF;
static const uint16_t DIM    = TFT_DARKGREY;

// Bond level: 0-1000 → 0-5 stars
static int bondStars(uint16_t bond) { return min(5, (int)(bond / 200)); }

static void drawStatRow(int y, const char* label, uint8_t val) {
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(12, y);
    M5.Lcd.printf("%-8s", label);

    int filled = map(val, 0, 100, 0, 140);
    uint16_t col = (val > 66) ? TFT_GREEN : (val > 33 ? TFT_YELLOW : TFT_RED);
    M5.Lcd.fillRect(75, y,         filled,       8, col);
    M5.Lcd.fillRect(75 + filled, y, 140 - filled, 8, DIM);

    M5.Lcd.setCursor(222, y);
    M5.Lcd.printf("%3d%%", val);
}

static const char* advisorMessage(const Pet& pet) {
    if (pet.health  < 30) return "I feel sick... please help me!";
    if (pet.hunger  < 20) return "So hungry... feed me please!";
    if (pet.hunger  < 40) return "Getting a bit peckish...";
    if (pet.happiness < 25) return "I'm bored and lonely...";
    if (pet.happiness < 50) return "Could use some playtime!";
    if (pet.health  < 50) return "Feeling a little under the weather.";
    if (pet.hunger > 80 && pet.happiness > 80 && pet.health > 80)
        return "Life is perfect! I love you!";
    if (pet.happiness > 70) return "I'm happy! Let's play more!";
    return "I'm doing okay. Thanks for caring!";
}

void showStatusReport(const Pet& pet, const Inventory& inv) {
    M5.Lcd.fillScreen(TFT_BLACK);

    // Header
    M5.Lcd.fillRect(0, 0, 320, 28, DIM);
    M5.Lcd.setTextColor(TFT_YELLOW, DIM);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 6);
    M5.Lcd.printf("%-10s Health Card", pet.name.c_str());

    M5.Lcd.drawFastHLine(0, 28, 320, BORDER);

    // Bond row
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Lcd.setCursor(12, 36);
    M5.Lcd.printf("Age: %-3d day(s)   Bond: ", pet.age);
    int stars = bondStars(inv.bond);
    M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    for (int i = 0; i < 5; i++) M5.Lcd.print(i < stars ? "*" : "-");
    M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Lcd.printf(" (%d/1000)", inv.bond);

    M5.Lcd.drawFastHLine(0, 50, 320, DIM);

    // Stat bars
    drawStatRow(58,  "Hunger",  pet.hunger);
    drawStatRow(76,  "Happy",   pet.happiness);
    drawStatRow(94,  "Health",  pet.health);

    M5.Lcd.drawFastHLine(0, 110, 320, DIM);

    // Summary row
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(12, 118);
    int ownedCount = 0;
    for (int i = 0; i < ITEM_COUNT; i++) if (inv.owned[i]) ownedCount++;
    M5.Lcd.printf("Coins: %-5d   Items: %d/%d", inv.coins, ownedCount, ITEM_COUNT);

    M5.Lcd.drawFastHLine(0, 132, 320, DIM);

    // Advisor
    M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Lcd.setCursor(12, 140);
    M5.Lcd.print("Advisor:");
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(12, 154);
    M5.Lcd.println(advisorMessage(pet));

    M5.Lcd.drawFastHLine(0, 205, 320, BORDER);
    M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Lcd.setCursor(80, 213);
    M5.Lcd.print("Press any button to close");

    // Wait for any button
    while (true) {
        M5.update();
        if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed()) break;
        delay(20);
    }
}

bool showConfirmDialog(const char* line1, const char* line2, const char* confirmLabel) {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.drawRect(40, 60, 240, 120, BORDER);

    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(50, 75);  M5.Lcd.print(line1);
    M5.Lcd.setCursor(50, 95);  M5.Lcd.print(line2);

    M5.Lcd.drawFastHLine(40, 140, 240, DIM);
    M5.Lcd.setTextColor(TFT_RED,  TFT_BLACK); M5.Lcd.setCursor(55,  150); M5.Lcd.print(confirmLabel);
    M5.Lcd.setTextColor(DIM,      TFT_BLACK); M5.Lcd.setCursor(220, 150); M5.Lcd.print("[Cancel]");

    while (true) {
        M5.update();
        if (M5.BtnA.wasPressed()) return true;
        if (M5.BtnC.wasPressed()) return false;
        delay(20);
    }
}
