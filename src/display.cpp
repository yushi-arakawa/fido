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

static const char* stageName(uint8_t age) {
    if (age < 20) return "Egg";
    if (age < 40) return "Child";
    if (age < 60) return "Teen";
    if (age < 80) return "Young";
    return "Elder";
}

static const char* moodLabel(PetMood m) {
    switch (m) {
        case PetMood::Happy:   return "Happy  :D";
        case PetMood::Hungry:  return "Hungry :(";
        case PetMood::Sleepy:  return "Sleepy zz";
        case PetMood::Sick:    return "Sick   xx";
        default:               return "Stable  .";
    }
}

// ── Status panel ─────────────────────────────────────────────────────────

static void drawStatusPanel(const Pet& pet, const Inventory& inv) {
    // Header strip
    M5.Lcd.fillRect(1, 1, PANEL_DIV - 2, 22, SM_HDR);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(STAT_X, 4);
    M5.Lcd.print(pet.name);

    // Day + stage + coins
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(STAT_X, 27);
    M5.Lcd.printf("Day%-3d[%s]", pet.age, stageName(pet.age));
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
    M5.Lcd.setCursor(STAT_X + 108, 27);
    M5.Lcd.printf("$%d", inv.coins);

    M5.Lcd.drawFastHLine(1, 37, PANEL_DIV - 2, SM_DIV);

    // Stat bars
    struct { const char* lbl; uint8_t val; } stats[3] = {
        {"Energy", pet.hunger},
        {"Morale", pet.happiness},
        {"Shield", pet.health},
    };
    for (int i = 0; i < 3; i++) {
        int y = 42 + i * 24;
        bool crit = stats[i].val < 30;
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(crit ? SM_WHITE : SM_GREY, SM_BG);
        M5.Lcd.setCursor(STAT_X, y);
        M5.Lcd.printf("%-6s%s", stats[i].lbl, crit ? "!" : " ");
        spBar(STAT_X, y + 10, BAR_W, 5, stats[i].val);
    }

    M5.Lcd.drawFastHLine(1, 118, PANEL_DIV - 2, SM_DIV);

    // Mood
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
    M5.Lcd.setCursor(STAT_X, 122);
    M5.Lcd.print(moodLabel(pet.mood));

    M5.Lcd.drawFastHLine(1, 133, PANEL_DIV - 2, SM_DIV);

    // Cargo
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(STAT_X, 137);
    M5.Lcd.print("Cargo:");
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
    int ix = STAT_X, iy = 147;
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
        M5.Lcd.setTextColor(SM_DIM, SM_BG);
        M5.Lcd.setCursor(STAT_X, 147);
        M5.Lcd.print("--");
    }
}

// ── Action panel ─────────────────────────────────────────────────────────

static const char* ACTION_LABELS[] = { "Feed", "Play", "Talk", "Game", "Shop" };

static void drawActionPanel(int selection) {
    // Header strip
    M5.Lcd.fillRect(1, 1, PANEL_DIV - 2, 22, SM_HDR);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(STAT_X, 4);
    M5.Lcd.print("Action");

    for (int i = 0; i < 5; i++) {
        int y = 28 + i * 24;
        bool sel = (i == selection);
        uint16_t rowBg = sel ? SM_SEL : SM_BG;
        M5.Lcd.fillRect(1, y, PANEL_DIV - 2, 22, rowBg);
        if (sel) {
            // Left edge accent
            M5.Lcd.fillRect(1, y, 2, 22, SM_WHITE);
        }
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(sel ? SM_WHITE : SM_GREY, rowBg);
        M5.Lcd.setCursor(STAT_X + 3, y + 3);
        M5.Lcd.printf("%s %s", sel ? ">" : " ", ACTION_LABELS[i]);
    }
}

// ── Borders ───────────────────────────────────────────────────────────────

static void drawAllBorders() {
    spCornerFrame(0,         0,     PANEL_DIV,       MSG_Y);
    spCornerFrame(PANEL_DIV, 0,     320 - PANEL_DIV, MSG_Y);
    spCornerFrame(0,         MSG_Y, 320,              MSG_H);
}

// ── Public API ────────────────────────────────────────────────────────────

void displayLeftPanel(const Pet& pet, UIMode mode, int selection, const Inventory& inv) {
    // Plain black fill — no per-pixel star drawing here (causes visible noise)
    M5.Lcd.fillRect(1, 1, PANEL_DIV - 2, MSG_Y - 2, SM_BG);

    if (mode == UIMode::Status) {
        drawStatusPanel(pet, inv);
    } else {
        drawActionPanel(selection);
    }

    charAnimRedraw();
    drawAllBorders();
}

void displayMessage(const String& msg) {
    M5.Lcd.fillRect(1, MSG_Y + 1, 318, MSG_H - 2, SM_HDR);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_LIGHT, SM_HDR);
    M5.Lcd.setCursor(6, MSG_Y + 8);
    M5.Lcd.print("> ");
    M5.Lcd.println(msg);
    spCornerFrame(0, MSG_Y, 320, MSG_H);
}

void displayMenu(UIMode mode, int /*selection*/) {
    M5.Lcd.fillRect(0, MENU_Y, 320, 240 - MENU_Y, SM_BG);
    M5.Lcd.drawFastHLine(0, MENU_Y, 320, SM_DIV);
    M5.Lcd.setTextSize(1);

    if (mode == UIMode::Status) {
        M5.Lcd.setTextColor(SM_GREY,  SM_BG); M5.Lcd.setCursor(10,  MENU_Y + 7); M5.Lcd.print("[A] Power");
        M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(115, MENU_Y + 7); M5.Lcd.print("[B] Card");
        M5.Lcd.setTextColor(SM_WHITE, SM_BG); M5.Lcd.setCursor(225, MENU_Y + 7); M5.Lcd.print("[C] Act");
    } else {
        M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(10,  MENU_Y + 7); M5.Lcd.print("[A] Move");
        M5.Lcd.setTextColor(SM_WHITE, SM_BG); M5.Lcd.setCursor(115, MENU_Y + 7); M5.Lcd.print("[B] OK");
        M5.Lcd.setTextColor(SM_GREY,  SM_BG); M5.Lcd.setCursor(225, MENU_Y + 7); M5.Lcd.print("[C] Back");
    }
}
