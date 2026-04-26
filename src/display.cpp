#include "display.h"
#include "char_anim.h"
#include <M5Stack.h>

static const int STAT_X    = 5;
static const int MSG_Y     = 155;
static const int MSG_H     = 58;
static const int MENU_Y    = 218;
static const int PANEL_DIV = 165;

static const uint16_t BORDER_COL = 0x05BF;

static void drawAllBorders() {
    M5.Lcd.drawRect(0,         0,     PANEL_DIV,       MSG_Y, BORDER_COL);
    M5.Lcd.drawRect(PANEL_DIV, 0,     320 - PANEL_DIV, MSG_Y, BORDER_COL);
    M5.Lcd.drawRect(0,         MSG_Y, 320,              MSG_H, BORDER_COL);
}

static const char* moodLabel(PetMood m) {
    switch (m) {
        case PetMood::Happy:   return "Happy :D";
        case PetMood::Hungry:  return "Hungry:(";
        case PetMood::Sleepy:  return "Sleepy zz";
        case PetMood::Sick:    return "Sick  xx";
        default:               return "Neutral.";
    }
}

static uint16_t barColor(uint8_t v) {
    if (v > 66) return GREEN;
    if (v > 33) return YELLOW;
    return RED;
}

static void drawBar(int x, int y, uint8_t val) {
    int filled = map(val, 0, 100, 0, 76);
    M5.Lcd.fillRect(x,          y, filled,      5, barColor(val));
    M5.Lcd.fillRect(x + filled, y, 76 - filled, 5, DARKGREY);
}

static void drawStatusPanel(const Pet& pet, const Inventory& inv) {
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(STAT_X, 6);
    M5.Lcd.print(pet.name);

    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(STAT_X, 26);
    M5.Lcd.printf("Day %-3d  $%d", pet.age, inv.coins);

    M5.Lcd.drawFastHLine(1, 36, PANEL_DIV - 2, DARKGREY);

    M5.Lcd.setCursor(STAT_X, 42); M5.Lcd.print("Hunger");
    drawBar(STAT_X, 52, pet.hunger);

    M5.Lcd.setCursor(STAT_X, 62); M5.Lcd.print("Happy");
    drawBar(STAT_X, 72, pet.happiness);

    M5.Lcd.setCursor(STAT_X, 82); M5.Lcd.print("Health");
    drawBar(STAT_X, 92, pet.health);

    M5.Lcd.drawFastHLine(1, 104, PANEL_DIV - 2, DARKGREY);

    M5.Lcd.setCursor(STAT_X, 108);
    M5.Lcd.printf("%-10s", moodLabel(pet.mood));

    M5.Lcd.drawFastHLine(1, 120, PANEL_DIV - 2, DARKGREY);

    // Owned items
    M5.Lcd.setCursor(STAT_X, 124);
    M5.Lcd.setTextColor(TFT_CYAN, BLACK);
    M5.Lcd.print("Items:");
    M5.Lcd.setTextColor(WHITE, BLACK);

    int ix = STAT_X, iy = 134;
    bool anyOwned = false;
    for (int i = 0; i < ITEM_COUNT; i++) {
        if (!inv.owned[i]) continue;
        anyOwned = true;
        int w = strlen(ITEM_DEFS[i].name) * 6 + 2;
        if (ix + w > PANEL_DIV - 4) { ix = STAT_X; iy += 10; }
        M5.Lcd.setCursor(ix, iy);
        M5.Lcd.print(ITEM_DEFS[i].name);
        ix += w + 2;
    }
    if (!anyOwned) {
        M5.Lcd.setCursor(STAT_X, 134);
        M5.Lcd.setTextColor(DARKGREY, BLACK);
        M5.Lcd.print("none");
        M5.Lcd.setTextColor(WHITE, BLACK);
    }
}

static const char* ACTION_LABELS[] = { "Feed", "Play", "Talk", "Game", "Shop" };
static const int   ACTION_COUNT    = 5;

static void drawActionPanel(int selection) {
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(STAT_X, 6);
    M5.Lcd.print("Action");
    M5.Lcd.setTextSize(1);

    for (int i = 0; i < ACTION_COUNT; i++) {
        int y = 36 + i * 22;
        bool sel = (i == selection);
        M5.Lcd.fillRect(1, y - 1, PANEL_DIV - 2, 20, sel ? DARKGREY : BLACK);
        M5.Lcd.setTextColor(sel ? YELLOW : WHITE, sel ? DARKGREY : BLACK);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setCursor(STAT_X, y);
        M5.Lcd.printf("%s %s", sel ? ">" : " ", ACTION_LABELS[i]);
    }
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(WHITE, BLACK);
}

void displayLeftPanel(const Pet& pet, UIMode mode, int selection, const Inventory& inv) {
    M5.Lcd.fillRect(1, 1, PANEL_DIV - 2, MSG_Y - 2, BLACK);

    if (mode == UIMode::Status) {
        drawStatusPanel(pet, inv);
    } else {
        drawActionPanel(selection);
    }

    charAnimRedraw();
    drawAllBorders();
}

void displayMessage(const String& msg) {
    M5.Lcd.fillRect(0, MSG_Y, 320, MSG_H, DARKGREY);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(WHITE, DARKGREY);
    M5.Lcd.setCursor(5, MSG_Y + 4);
    M5.Lcd.println(msg);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.drawRect(0, MSG_Y, 320, MSG_H, BORDER_COL);
}

void displayMenu(UIMode mode, int /*selection*/) {
    M5.Lcd.fillRect(0, MENU_Y, 320, 22, BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(WHITE, BLACK);

    if (mode == UIMode::Status) {
        M5.Lcd.setCursor(10,  MENU_Y + 4); M5.Lcd.print("[Pwr]");
        M5.Lcd.setCursor(130, MENU_Y + 4); M5.Lcd.print("[Card]");
        M5.Lcd.setCursor(240, MENU_Y + 4); M5.Lcd.print("[Action]");
    } else {
        M5.Lcd.setCursor(10,  MENU_Y + 4); M5.Lcd.print("[Move]");
        M5.Lcd.setCursor(130, MENU_Y + 4); M5.Lcd.print("[OK]");
        M5.Lcd.setCursor(240, MENU_Y + 4); M5.Lcd.print("[Status]");
    }
}
