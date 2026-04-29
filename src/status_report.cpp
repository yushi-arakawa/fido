#include "status_report.h"
#include "space_ui.h"
#include <M5Stack.h>
#include <Preferences.h>

// ── Volume helpers ────────────────────────────────────────────────────────

static uint8_t loadVolume() {
    Preferences p;
    p.begin("fido", true);
    uint8_t v = p.getUChar("vol", 2);
    p.end();
    return v;
}

static void saveVolume(uint8_t v) {
    Preferences p;
    p.begin("fido", false);
    p.putUChar("vol", v);
    p.end();
    // ~3dB per step, max=15 (perceptually even spacing)
    static const uint8_t VOL_MAP[9] = {0, 1, 1, 1, 2, 2, 3, 4, 6};
    M5.Speaker.setVolume(VOL_MAP[v]);
}

// ── Confirm dialog ────────────────────────────────────────────────────────

bool showConfirmDialog(const char* line1, const char* line2, const char* confirmLabel) {
    spDrawBackground();
    spDrawStarfield(0, 0, 320, 240);

    M5.Lcd.fillRect(30, 55, 260, 130, SM_HDR);
    spCornerFrame(30, 55, 260, 130);

    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_LIGHT, SM_HDR);
    M5.Lcd.setCursor(44, 74);  M5.Lcd.print(line1);
    if (line2 && line2[0]) {
        M5.Lcd.setCursor(44, 90); M5.Lcd.print(line2);
    }
    M5.Lcd.drawFastHLine(30, 148, 260, SM_DIV);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setCursor(44,  158); M5.Lcd.print(confirmLabel);
    M5.Lcd.setTextColor(SM_DIM, SM_HDR);
    M5.Lcd.setCursor(210, 158); M5.Lcd.print("[C] Cancel");

    while (true) {
        M5.update();
        if (M5.BtnA.wasPressed()) { M5.Speaker.tone(330, 80); return true; }
        if (M5.BtnC.wasPressed()) { M5.Speaker.tone(262, 60); return false; }
        delay(20);
    }
}

// ── Settings screen ───────────────────────────────────────────────────────

static void drawSettings(int sel, uint8_t vol) {
    // Header
    M5.Lcd.fillRect(0, 0, 320, 22, SM_HDR);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(8, 4);
    M5.Lcd.print("Config");

    // Items
    struct { const char* label; } items[3] = {
        {"Sound Volume"},
        {"Power Off"},
        {"Clear All Data"},
    };

    for (int i = 0; i < 3; i++) {
        int y = 30 + i * 40;
        bool active = (i == sel);
        uint16_t bg = active ? SM_SEL : SM_BG;
        M5.Lcd.fillRect(0, y, 320, 38, bg);
        if (active) M5.Lcd.fillRect(0, y, 3, 38, SM_WHITE);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(active ? SM_WHITE : SM_GREY, bg);
        M5.Lcd.setCursor(12, y + 6);
        M5.Lcd.printf("%s %s", active ? ">" : " ", items[i].label);

        if (i == 0) {
            // Volume bar
            M5.Lcd.setTextColor(SM_DIM, bg);
            M5.Lcd.setCursor(12, y + 20);
            for (int b = 0; b < 8; b++) M5.Lcd.print(b < vol ? "#" : "-");
            M5.Lcd.printf("  %d/8", vol);
        }
    }

    spCornerFrame(0, 0, 320, 160);

    // Footer
    M5.Lcd.fillRect(0, 218, 320, 22, SM_BG);
    M5.Lcd.drawFastHLine(0, 218, 320, SM_DIV);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(10,  225); M5.Lcd.print("[A] Move");
    M5.Lcd.setTextColor(SM_WHITE, SM_BG); M5.Lcd.setCursor(115, 225); M5.Lcd.print("[B] Select");
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(225, 225); M5.Lcd.print("[C] Close");
}

void showSettings(Pet& pet, Inventory& inv) {
    spDrawBackground();
    spDrawStarfield(0, 0, 320, 240);

    int     sel = 0;
    uint8_t vol = loadVolume();
    drawSettings(sel, vol);

    while (true) {
        M5.update();

        if (M5.BtnA.wasPressed()) {
            M5.Speaker.tone(262, 60); // C4
            sel = (sel + 1) % 3;
            drawSettings(sel, vol);
        }

        if (M5.BtnB.wasPressed()) {
            if (sel == 0) {
                // Adjust volume — play test tone at new level so user can hear it
                vol = (vol + 1) % 9; // 0-8
                saveVolume(vol);
                M5.Speaker.tone(440, 150); // A4 preview
                drawSettings(sel, vol);
            } else if (sel == 1) {
                // Power off
                if (showConfirmDialog("Power off?", "", "[A] Power Off")) {
                    M5.Power.powerOFF();
                }
                // Redraw settings if cancelled
                spDrawBackground();
                spDrawStarfield(0, 0, 320, 240);
                drawSettings(sel, vol);
            } else if (sel == 2) {
                // Clear data
                if (showConfirmDialog("Clear all data?",
                                      "This cannot be undone.",
                                      "[A] Confirm")) {
                    M5.Speaker.end(); // stop tone before reset
                    Preferences p;
                    p.begin("fido", false);
                    p.clear();
                    p.end();
                    ESP.restart();
                }
                spDrawBackground();
                spDrawStarfield(0, 0, 320, 240);
                drawSettings(sel, vol);
            }
        }

        if (M5.BtnC.wasPressed()) { M5.Speaker.tone(392, 60); break; } // G4

        delay(20);
    }
}
