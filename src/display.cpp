#include "display.h"
#include "char_anim.h"
#include "space_ui.h"
#include <M5Stack.h>

static const int STAT_X    = 5;
static const int MSG_Y     = 155;
static const int MSG_H     = 58;
static const int MENU_Y    = 218;
static const int PANEL_DIV = 165;
static const int BAR_W     = PANEL_DIV - STAT_X * 2 - 3; // 152

// ── Helpers ───────────────────────────────────────────────────────────────

static const char* moodLabel(PetMood m) {
    switch (m) {
        case PetMood::Happy:   return "** HAPPY **";
        case PetMood::Hungry:  return "!! HUNGRY !";
        case PetMood::Sleepy:  return "~~ SLEEPY ~";
        case PetMood::Sick:    return "xx  SICK  x";
        default:               return "--  STABLE -";
    }
}

static uint16_t moodColor(PetMood m) {
    switch (m) {
        case PetMood::Happy:   return SP_NEON_C;
        case PetMood::Hungry:  return SP_NEON_Y;
        case PetMood::Sleepy:  return (uint16_t)0x7BEF;
        case PetMood::Sick:    return SP_NEON_R;
        default:               return (uint16_t)0x8410;
    }
}

// ── Status panel ─────────────────────────────────────────────────────────

static void drawStatusPanel(const Pet& pet, const Inventory& inv) {
    // Header strip
    M5.Lcd.fillRect(1, 1, PANEL_DIV - 2, 22, SP_HDR_BG);
    M5.Lcd.setTextColor(SP_NEON_C, SP_HDR_BG);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(STAT_X, 4);
    M5.Lcd.print(pet.name);

    // Day + coins row
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor((uint16_t)0x9492, TFT_BLACK);
    M5.Lcd.setCursor(STAT_X, 26);
    M5.Lcd.printf("DAY:%-2d", pet.age);
    M5.Lcd.setTextColor(SP_NEON_Y, TFT_BLACK);
    M5.Lcd.setCursor(STAT_X + 48, 26);
    M5.Lcd.printf("$%d", inv.coins);

    M5.Lcd.drawFastHLine(1, 36, PANEL_DIV - 2, SP_GRID);

    // Stat bars
    struct { const char* lbl; uint8_t val; } stats[3] = {
        {"ENERGY", pet.hunger},
        {"MORALE", pet.happiness},
        {"SHIELD", pet.health},
    };
    for (int i = 0; i < 3; i++) {
        int y = 41 + i * 24;
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(SP_DIM_C, TFT_BLACK);
        M5.Lcd.setCursor(STAT_X, y);
        M5.Lcd.print(stats[i].lbl);
        spNeonBar(STAT_X, y + 10, BAR_W, 6, stats[i].val, spStatColor(stats[i].val));
    }

    M5.Lcd.drawFastHLine(1, 116, PANEL_DIV - 2, SP_GRID);

    // Mood
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(moodColor(pet.mood), TFT_BLACK);
    M5.Lcd.setCursor(STAT_X, 120);
    M5.Lcd.print(moodLabel(pet.mood));

    M5.Lcd.drawFastHLine(1, 131, PANEL_DIV - 2, SP_GRID);

    // Cargo (owned items)
    M5.Lcd.setTextColor(SP_NEON_M, TFT_BLACK);
    M5.Lcd.setCursor(STAT_X, 135);
    M5.Lcd.print("CARGO:");
    M5.Lcd.setTextColor((uint16_t)0xCE59, TFT_BLACK);
    int ix = STAT_X, iy = 145;
    bool any = false;
    for (int i = 0; i < ITEM_COUNT; i++) {
        if (!inv.owned[i]) continue;
        any = true;
        int w = strlen(ITEM_DEFS[i].name) * 6 + 2;
        if (ix + w > PANEL_DIV - 4) { ix = STAT_X; iy += 10; }
        if (iy > 152) break;
        M5.Lcd.setCursor(ix, iy);
        M5.Lcd.print(ITEM_DEFS[i].name);
        ix += w + 2;
    }
    if (!any) {
        M5.Lcd.setTextColor(SP_GRID, TFT_BLACK);
        M5.Lcd.setCursor(STAT_X, 145);
        M5.Lcd.print("[empty]");
    }
}

// ── Action panel ─────────────────────────────────────────────────────────

static const char* ACTION_LABELS[] = { "Feed", "Play", "Talk", "Game", "Shop" };

static void drawActionPanel(int selection) {
    // Header strip
    M5.Lcd.fillRect(1, 1, PANEL_DIV - 2, 22, SP_HDR_BG);
    M5.Lcd.setTextColor(SP_NEON_M, SP_HDR_BG);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(STAT_X, 4);
    M5.Lcd.print("ACTION");

    for (int i = 0; i < 5; i++) {
        int y = 28 + i * 24;
        bool sel = (i == selection);
        uint16_t rowBg = sel ? SP_HDR_BG : TFT_BLACK;
        M5.Lcd.fillRect(1, y, PANEL_DIV - 2, 22, rowBg);
        if (sel) {
            M5.Lcd.fillRect(1, y, 3, 22, SP_NEON_M); // neon left-edge accent
        }
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(sel ? SP_NEON_Y : (uint16_t)0x7BEF, rowBg);
        M5.Lcd.setCursor(STAT_X + 4, y + 3);
        M5.Lcd.printf("%s %s", sel ? ">" : " ", ACTION_LABELS[i]);
    }
}

// ── Borders ───────────────────────────────────────────────────────────────

static void drawAllBorders() {
    spCornerFrame(0,          0,     PANEL_DIV,       MSG_Y, SP_NEON_C);
    spCornerFrame(PANEL_DIV,  0,     320 - PANEL_DIV, MSG_Y, SP_NEON_M);
    spCornerFrame(0,          MSG_Y, 320,              MSG_H, (uint16_t)0x780F);
}

// ── Public API ────────────────────────────────────────────────────────────

void displayLeftPanel(const Pet& pet, UIMode mode, int selection, const Inventory& inv) {
    M5.Lcd.fillRect(1, 1, PANEL_DIV - 2, MSG_Y - 2, TFT_BLACK);
    spDrawStarfield(1, 1, PANEL_DIV - 2, MSG_Y - 2);

    if (mode == UIMode::Status) {
        drawStatusPanel(pet, inv);
    } else {
        drawActionPanel(selection);
    }

    charAnimRedraw();
    drawAllBorders();
}

void displayMessage(const String& msg) {
    // Nebula scan-line background
    for (int y = MSG_Y + 1; y < MSG_Y + MSG_H - 1; y++) {
        M5.Lcd.drawFastHLine(1, y, 318, (y & 1) ? SP_MSG_BG : (uint16_t)0x1004);
    }
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SP_NEON_C, SP_MSG_BG);
    M5.Lcd.setCursor(6, MSG_Y + 8);
    M5.Lcd.print("> ");
    M5.Lcd.println(msg);
    spCornerFrame(0, MSG_Y, 320, MSG_H, (uint16_t)0x780F);
}

void displayMenu(UIMode mode, int /*selection*/) {
    M5.Lcd.fillRect(0, MENU_Y, 320, 240 - MENU_Y, TFT_BLACK);
    // Top separator
    M5.Lcd.drawFastHLine(0, MENU_Y, 320, SP_GRID);
    M5.Lcd.setTextSize(1);

    if (mode == UIMode::Status) {
        M5.Lcd.setTextColor(SP_NEON_R, TFT_BLACK);
        M5.Lcd.setCursor(10,  MENU_Y + 7); M5.Lcd.print("[A:PWR]");
        M5.Lcd.setTextColor(SP_NEON_C, TFT_BLACK);
        M5.Lcd.setCursor(115, MENU_Y + 7); M5.Lcd.print("[B:CARD]");
        M5.Lcd.setTextColor(SP_NEON_M, TFT_BLACK);
        M5.Lcd.setCursor(225, MENU_Y + 7); M5.Lcd.print("[C:ACT]");
    } else {
        M5.Lcd.setTextColor(SP_NEON_G, TFT_BLACK);
        M5.Lcd.setCursor(10,  MENU_Y + 7); M5.Lcd.print("[A:MOVE]");
        M5.Lcd.setTextColor(SP_NEON_Y, TFT_BLACK);
        M5.Lcd.setCursor(115, MENU_Y + 7); M5.Lcd.print("[B:OK]");
        M5.Lcd.setTextColor(SP_NEON_M, TFT_BLACK);
        M5.Lcd.setCursor(225, MENU_Y + 7); M5.Lcd.print("[C:BACK]");
    }
}
