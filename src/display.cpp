#include "display.h"
#include "space_ui.h"
#include "nasa_gacha.h"
#include <M5Stack.h>

static const int MSG_Y  = 155;
static const int MSG_H  = 63;   // 155+63 = 218
static const int MENU_Y = 218;
static const int MENU_H = 22;
static const int PDIV   = 165;  // left/right panel split in Act mode
static const int SX     = 8;    // stat text x

// ── Fido tips (right panel in Act mode) ──────────────────────────────────

static const char* TIPS_L1[10] = {
    "Feeds on stray photons",
    "Stretches 3 light-years",
    "Purring generates",
    "Sheds dark matter",
    "Sleeps by folding into",
    "Sees across 17",
    "Fave toy: collapsed",
    "Its meows travel",
    "Hairballs contain",
    "Tail wags slightly",
};
static const char* TIPS_L2[10] = {
    "near event horizons.",
    "when yawning.",
    "micro-singularities.",
    "instead of fur.",
    "a 2D manifold.",
    "dimensions at once.",
    "neutron star (small).",
    "faster than light.",
    "compressed spacetime.",
    "warp local gravity.",
};

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

static const char* advisorMsg(const Pet& pet) {
    if (pet.health    < 30) return "Hull breach! Help me!";
    if (pet.hunger    < 20) return "Fuel critical. Feed me!";
    if (pet.hunger    < 40) return "Running low on fuel...";
    if (pet.happiness < 25) return "Crew morale very low.";
    if (pet.happiness < 50) return "Need recreation time.";
    if (pet.health    < 50) return "Shields at half.";
    if (pet.hunger > 80 && pet.happiness > 80 && pet.health > 80)
        return "All systems nominal!";
    if (pet.happiness > 70) return "Happy and exploring!";
    return "Systems stable.";
}

// ── Act content (noise-safe: no background touch) ────────────────────────

static const char* ACT_LABELS[] = { "Feed", "Play", "Game", "Shop" };

void displayActContent(int sel) {
    Serial.printf("[ACT] selected: %s\n", ACT_LABELS[sel]);
    // Left panel: action list
    M5.Lcd.fillRect(0, 0, PDIV, MSG_Y, SM_BG);
    M5.Lcd.fillRect(0, 0, PDIV, 22, SM_HDR);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(SX, 4);
    M5.Lcd.print("Actions");

    for (int i = 0; i < 4; i++) {
        int y = 22 + i * 33;
        bool active = (i == sel);
        uint16_t bg = active ? SM_SEL : SM_BG;
        M5.Lcd.fillRect(0, y, PDIV, 33, bg);
        if (active) M5.Lcd.fillRect(0, y, 3, 33, SM_WHITE);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(active ? SM_WHITE : SM_GREY, bg);
        M5.Lcd.setCursor(SX + 4, y + 8);
        M5.Lcd.printf("%s %s", active ? ">" : " ", ACT_LABELS[i]);
    }
    spCornerFrame(0, 0, PDIV, MSG_Y);

    // Right panel: Fido tips
    int RX = PDIV + 8;
    int RW = 320 - PDIV;
    M5.Lcd.fillRect(PDIV, 0, RW, MSG_Y, SM_BG);
    M5.Lcd.fillRect(PDIV, 0, RW, 22, SM_HDR);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(RX, 7);
    M5.Lcd.print("FIDO FACTS");

    uint8_t tip = (millis() / 10000) % 10;
    M5.Lcd.setTextColor(SM_DIM, SM_BG);
    M5.Lcd.setCursor(RX, 30);
    M5.Lcd.printf("tip %d/10", tip + 1);

    M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
    M5.Lcd.setCursor(RX, 50);
    M5.Lcd.print(TIPS_L1[tip]);
    M5.Lcd.setCursor(RX, 62);
    M5.Lcd.print(TIPS_L2[tip]);

    M5.Lcd.drawFastHLine(PDIV + 4, 80, RW - 8, SM_DIV);

    M5.Lcd.setTextColor(SM_DIM, SM_BG);
    M5.Lcd.setCursor(RX, 90);
    M5.Lcd.print("Blackhole Cat Fido");
    M5.Lcd.setCursor(RX, 102);
    M5.Lcd.print("Ecology Database");
    M5.Lcd.setCursor(RX, 114);
    M5.Lcd.print("v2.4.1  classified");

    spCornerFrame(PDIV, 0, RW, MSG_Y);
}

// ── Back content (noise-safe) ─────────────────────────────────────────────

void displayBackContent(const Pet& pet, const Inventory& inv, const NasaCargo& nasa) {
    int owned = 0;
    for (int i = 0; i < ITEM_COUNT; i++) if (inv.owned[i]) owned++;
    Serial.printf("[BACK] %s [%s] Day%d | Energy:%d Morale:%d Shield:%d | Mood:%s | Bond:%d/1000 $%d Items:%d/%d | %s\n",
        pet.name.c_str(), stageName(pet.age), pet.age,
        pet.hunger, pet.happiness, pet.health,
        moodLabel(pet.mood), inv.bond, inv.coins, owned, ITEM_COUNT,
        advisorMsg(pet));
    M5.Lcd.fillRect(0, 0, 320, MENU_Y, SM_BG);

    // Header
    M5.Lcd.fillRect(0, 0, 320, 22, SM_HDR);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(SX, 4);
    M5.Lcd.printf("%-5s [%s] Day%-3d", pet.name.c_str(), stageName(pet.age), pet.age);

    // Bond + coins
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(SX, 28);
    M5.Lcd.print("Bond: ");
    int stars = min(5, (int)(inv.bond / 200));
    M5.Lcd.setTextColor(SM_WHITE, SM_BG);
    for (int i = 0; i < 5; i++) M5.Lcd.print(i < stars ? "*" : "-");
    M5.Lcd.setTextColor(SM_DIM, SM_BG);
    M5.Lcd.printf(" (%d/1000)", inv.bond);
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
    M5.Lcd.setCursor(240, 28);
    M5.Lcd.printf("$%d", inv.coins);
    M5.Lcd.drawFastHLine(0, 40, 320, SM_DIV);

    // Stat bars
    struct { const char* lbl; uint8_t val; } stats[3] = {
        {"Energy", pet.hunger},
        {"Morale", pet.happiness},
        {"Shield", pet.health},
    };
    for (int i = 0; i < 3; i++) {
        int y = 46 + i * 22;
        bool crit = stats[i].val < 30;
        M5.Lcd.setTextColor(crit ? SM_WHITE : SM_GREY, SM_BG);
        M5.Lcd.setCursor(SX, y);
        M5.Lcd.printf("%-7s%s", stats[i].lbl, crit ? "!" : " ");
        spBar(70, y, 200, 8, stats[i].val);
        M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
        M5.Lcd.setCursor(276, y);
        M5.Lcd.printf("%3d%%", stats[i].val);
    }
    M5.Lcd.drawFastHLine(0, 114, 320, SM_DIV);

    // Mood + item count
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
    M5.Lcd.setCursor(SX, 120);
    M5.Lcd.print(moodLabel(pet.mood));
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(150, 120);
    M5.Lcd.printf("Items: %d/%d", owned, ITEM_COUNT);
    M5.Lcd.drawFastHLine(0, 132, 320, SM_DIV);

    // Cargo
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(SX, 138);
    M5.Lcd.print("Cargo: ");
    int cx = SX + 42, cy = 138;
    bool any = false;
    for (int i = 0; i < ITEM_COUNT; i++) {
        if (!inv.owned[i]) continue;
        any = true;
        int w = strlen(ITEM_DEFS[i].name) * 6 + 4;
        if (cx + w > 314) { cx = SX + 42; cy += 10; }
        if (cy > 150) break;
        M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
        M5.Lcd.setCursor(cx, cy);
        M5.Lcd.print(ITEM_DEFS[i].name);
        cx += w;
    }
    if (!any) {
        M5.Lcd.setTextColor(SM_DIM, SM_BG);
        M5.Lcd.setCursor(SX + 42, 138);
        M5.Lcd.print("--");
    }
    M5.Lcd.drawFastHLine(0, 160, 320, SM_DIV);

    // Advisor
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(SX, 166);
    M5.Lcd.print("Advisor: ");
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
    M5.Lcd.print(advisorMsg(pet));

    // NASA gacha cargo
    M5.Lcd.drawFastHLine(0, 178, 320, SM_DIV);
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(SX, 184);
    M5.Lcd.print("Space: ");
    if (nasa.count == 0) {
        M5.Lcd.setTextColor(SM_DIM, SM_BG);
        M5.Lcd.setCursor(SX + 42, 184);
        M5.Lcd.print("--");
    } else {
        int ncx = SX + 42, ncy = 184;
        for (uint8_t i = 0; i < nasa.count; i++) {
            int w = strlen(nasa.items[i]) * 6 + 4;
            if (ncx + w > 314) { ncx = SX + 42; ncy += 10; }
            if (ncy > 206) break;
            M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
            M5.Lcd.setCursor(ncx, ncy);
            M5.Lcd.print(nasa.items[i]);
            ncx += w;
        }
    }

    spCornerFrame(0, 0, 320, MENU_Y);
}

// ── Message box ───────────────────────────────────────────────────────────

void displayMessage(const String& msg) {
    Serial.printf("[MSG] %s\n", msg.c_str());
    M5.Lcd.fillRect(1, MSG_Y + 1, 318, MSG_H - 2, SM_HDR);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_LIGHT, SM_HDR);
    M5.Lcd.setCursor(6, MSG_Y + 8);
    M5.Lcd.print("> ");
    M5.Lcd.println(msg);
    spCornerFrame(0, MSG_Y, 320, MSG_H);
}

// ── Bottom menu bar ───────────────────────────────────────────────────────

void displayMenuBar(UIMode mode, int /*sel*/) {
    M5.Lcd.fillRect(0, MENU_Y, 320, MENU_H, SM_BG);
    M5.Lcd.drawFastHLine(0, MENU_Y, 320, SM_DIV);
    M5.Lcd.setTextSize(1);

    switch (mode) {
        case UIMode::Main:
            M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(10,  MENU_Y + 7); M5.Lcd.print("[A] Gacha");
            M5.Lcd.setTextColor(SM_WHITE, SM_BG); M5.Lcd.setCursor(115, MENU_Y + 7); M5.Lcd.print("[B] Talk");
            M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(225, MENU_Y + 7); M5.Lcd.print("[C] Act");
            break;
        case UIMode::Act:
            M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(10,  MENU_Y + 7); M5.Lcd.print("[A] Move");
            M5.Lcd.setTextColor(SM_WHITE, SM_BG); M5.Lcd.setCursor(115, MENU_Y + 7); M5.Lcd.print("[B] OK");
            M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(225, MENU_Y + 7); M5.Lcd.print("[C] Back");
            break;
        case UIMode::Back:
            M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(10,  MENU_Y + 7); M5.Lcd.print("[A] Config");
            M5.Lcd.setTextColor(SM_DIM,   SM_BG); M5.Lcd.setCursor(115, MENU_Y + 7); M5.Lcd.print("[B] ---");
            M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(225, MENU_Y + 7); M5.Lcd.print("[C] Main");
            break;
    }
}

// ── Full init (background + all content) ─────────────────────────────────

void displayInit(UIMode mode, const Pet& pet, const Inventory& inv, const NasaCargo& nasa, int sel) {
    spDrawBackground();
    spDrawStarfield(0, 0, 320, 240);

    switch (mode) {
        case UIMode::Main:
            displayMessage("");
            break;
        case UIMode::Act:
            displayActContent(sel);
            displayMessage("");
            break;
        case UIMode::Back:
            displayBackContent(pet, inv, nasa);
            break;
    }
    displayMenuBar(mode, sel);
}
