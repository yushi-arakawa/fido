#include <M5Stack.h>
#include "pet.h"
#include "display.h"
#include "claude_client.h"
#include "char_anim.h"
#include "game_data.h"
#include "minigame.h"
#include "shop.h"
#include "status_report.h"
#include <Preferences.h>

static Pet       pet       = {"Fido", 80, 80, 100, 0, PetMood::Happy};
static Inventory inv       = {0, 0, {false, false, false, false}};
static UIMode    uiMode    = UIMode::Status;
static int       actionIdx = 0;

static unsigned long lastTick  = 0;
static unsigned long aPressTime = 0; // for long-press detection
static const unsigned long TICK_MS     = 30000;
static const unsigned long LONGPRESS_MS = 2000;

static void refresh() {
    displayLeftPanel(pet, uiMode, actionIdx, inv);
    displayMenu(uiMode, actionIdx);
}

// Full redraw after a fullscreen mode (mini-game / shop).
static void returnFromFullscreen(const String& msg) {
    M5.Lcd.fillScreen(BLACK);
    refresh();
    charAnimUpdate(pet.age); // draw character immediately without waiting for next loop tick
    displayMessage(msg);
}

static void doAction(int idx) {
    switch (idx) {
        case 0: // Feed
            pet.hunger    = min(100, (int)pet.hunger    + 30);
            pet.happiness = min(100, (int)pet.happiness + 5);
            pet.mood      = pet.calcMood();
            refresh();
            displayMessage("Yum! Nom nom nom...");
            break;
        case 1: // Play
            pet.happiness = min(100, (int)pet.happiness + 20);
            pet.hunger    = max(0,   (int)pet.hunger    - 10);
            pet.mood      = pet.calcMood();
            refresh();
            displayMessage("Wheee! So fun!");
            break;
        case 2: // Talk
            displayMessage("...");
            displayMessage(askClaude(pet, "How are you feeling?"));
            break;
        case 3: { // Game
            uint16_t earned = runGameMenu();
            inv.coins += earned;
            saveAll(pet, inv);
            returnFromFullscreen("Game over! +" + String(earned) + " coins");
            break;
        }
        case 4: // Shop
            runShop(pet, inv);
            returnFromFullscreen("Thanks for shopping!");
            break;
    }
    displayMenu(uiMode, actionIdx);
}

void setup() {
    M5.begin();
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.fillScreen(BLACK);
    loadAll(pet, inv);
    lastTick = millis();
    refresh();
    displayMessage("Hello! I'm " + pet.name + "!");
}

void loop() {
    M5.update();

    // ── Right button: toggle Status / Action ────────────────────────────
    if (M5.BtnC.wasPressed()) {
        uiMode = (uiMode == UIMode::Status) ? UIMode::Action : UIMode::Status;
        refresh();
    }

    // ── Status mode buttons ─────────────────────────────────────────────
    if (uiMode == UIMode::Status) {
        // A: track press time for long-press
        if (M5.BtnA.wasPressed()) {
            aPressTime = millis();
            displayMessage("Hold 2s to clear data  Release to power off");
        }
        if (M5.BtnA.wasReleased() && aPressTime > 0) {
            unsigned long held = millis() - aPressTime;
            aPressTime = 0;
            if (held >= LONGPRESS_MS) {
                // Long press: clear data + restart
                if (showConfirmDialog("Clear all data and restart?",
                                      "This cannot be undone.",
                                      "[A] Confirm")) {
                    Preferences p;
                    p.begin("fido", false);
                    p.clear();
                    p.end();
                    ESP.restart();
                }
            } else {
                // Short press: power off
                if (showConfirmDialog("Power off?", "",
                                      "[A] Power Off")) {
                    M5.Power.powerOFF();
                }
            }
            returnFromFullscreen("");
        }

        // B: show health card
        if (M5.BtnB.wasPressed()) {
            showStatusReport(pet, inv);
            returnFromFullscreen("");
        }
    }

    // ── Action mode buttons ─────────────────────────────────────────────
    if (uiMode == UIMode::Action) {
        if (M5.BtnA.wasPressed()) {
            actionIdx = (actionIdx + 1) % 5;
            refresh();
        }
        if (M5.BtnB.wasPressed()) {
            doAction(actionIdx);
        }
    }

    // ── Game tick ───────────────────────────────────────────────────────
    if (millis() - lastTick >= TICK_MS) {
        lastTick = millis();
        pet.tick();
        if (inv.bond < 1000) inv.bond++;
        saveAll(pet, inv);
        refresh();
    }

    charAnimUpdate(pet.age);
    delay(16);
}
