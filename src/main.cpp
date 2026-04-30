#include <M5Stack.h>
#include "pet.h"
#include "display.h"
#include "claude_client.h"
#include "char_anim.h"
#include "game_data.h"
#include "minigame.h"
#include "shop.h"
#include "status_report.h"
#include "nasa_gacha.h"
#include <Preferences.h>

// Character center on Main screen (full-width, message box at y=155)
static const int CHAR_CX = 160;
static const int CHAR_CY = 80;

static Pet       pet       = {"Fido", 80, 80, 100, 0, PetMood::Happy};
static Inventory inv       = {0, 0, {false, false, false, false}};
static NasaCargo nasaCargo = {};
static UIMode    uiMode    = UIMode::Main;
static int       actSel    = 0;

static unsigned long lastTick = 0;
static const unsigned long TICK_MS = 30000;

// Redraw the current mode from scratch (background + content).
// Called on mode switch and after returning from a fullscreen sub-screen.
static void fullRedraw(const String& msg = "") {
    charAnimRedraw();
    displayInit(uiMode, pet, inv, nasaCargo, actSel);
    if (msg.length() > 0) displayMessage(msg);
}

// ── Action execution (Act mode) ───────────────────────────────────────────

static void doAct(int idx) {
    switch (idx) {
        case 0: // Feed
            pet.hunger    = min(100, (int)pet.hunger    + 30);
            pet.happiness = min(100, (int)pet.happiness + 5);
            pet.mood      = pet.calcMood();
            displayMessage("Yum! Nom nom nom...");
            break;
        case 1: // Play
            pet.happiness = min(100, (int)pet.happiness + 20);
            pet.hunger    = max(0,   (int)pet.hunger    - 10);
            pet.mood      = pet.calcMood();
            displayMessage("Wheee! So fun!");
            break;
        case 2: { // Game
            uint16_t earned = runGameMenu();
            inv.coins += earned;
            saveAll(pet, inv);
            fullRedraw("Game over! +" + String(earned) + " coins");
            break;
        }
        case 3: // Shop
            runShop(pet, inv);
            saveAll(pet, inv);
            fullRedraw("Thanks for shopping!");
            break;
    }
}

// ── Setup ─────────────────────────────────────────────────────────────────

void setup() {
    M5.begin();
    loadAll(pet, inv);
    loadNasaCargo(nasaCargo);
    lastTick = millis();
    {
        Preferences p;
        p.begin("fido", true);
        uint8_t vol = p.getUChar("vol", 1); // default: ON
        p.end();
        M5.Speaker.setVolume(vol ? 1 : 0);
    }
    charAnimPlayStartup();
    fullRedraw("Hello! I'm " + pet.name + "!");
}

// ── Main loop ─────────────────────────────────────────────────────────────

void loop() {
    M5.update();

    // ── C: cycle Main → Act → Back → Main ──────────────────────────────
    if (M5.BtnC.wasPressed()) {
        M5.Speaker.tone(196, 60); // G3
        if      (uiMode == UIMode::Main) uiMode = UIMode::Act;
        else if (uiMode == UIMode::Act)  uiMode = UIMode::Back;
        else                             uiMode = UIMode::Main;
        const char* modeNames[] = {"Main", "Act", "Back"};
        Serial.printf("[MODE] -> %s\n", modeNames[(int)uiMode]);
        fullRedraw();
    }

    // ── Main mode ───────────────────────────────────────────────────────
    if (uiMode == UIMode::Main) {
        // A: NASA gacha
        if (M5.BtnA.wasPressed()) {
            M5.Speaker.tone(131, 60); // C3
            displayMessage("Scanning space...");
            M5.Speaker.mute(); // tone duration is polled in M5.update(); stop before blocking WiFi
            String item = runNasaGacha(nasaCargo);
            if (item == "Cargo full!") {
                displayMessage("Space cargo full! (8/8)");
            } else if (item.length() > 0) {
                saveNasaCargo(nasaCargo);
                displayMessage("Discovery: " + item);
            } else {
                displayMessage("No signal...");
            }
        }

        // B: Talk (egg stage cannot talk)
        if (M5.BtnB.wasPressed()) {
            if (pet.age < 4) {
                displayMessage("...(still an egg)");
            } else {
                M5.Speaker.tone(165, 60); // E3
                displayMessage("...");
                displayMessage(askClaude(pet, "How are you feeling?"));
            }
        }
    }

    // ── Act mode ────────────────────────────────────────────────────────
    if (uiMode == UIMode::Act) {
        if (M5.BtnA.wasPressed()) {
            M5.Speaker.tone(131, 60); // C3
            actSel = (actSel + 1) % 4;
            displayActContent(actSel);
            displayMenuBar(uiMode, actSel);
        }
        if (M5.BtnB.wasPressed()) {
            M5.Speaker.tone(165, 60); // E3
            doAct(actSel);
        }
    }

    // ── Back mode ───────────────────────────────────────────────────────
    if (uiMode == UIMode::Back) {
        // A: Config / Settings
        if (M5.BtnA.wasPressed()) {
            M5.Speaker.tone(131, 60); // C3
            showSettings(pet, inv);
            fullRedraw();
        }
        // B: reserved
    }

    // ── Game tick ───────────────────────────────────────────────────────
    if (millis() - lastTick >= TICK_MS) {
        lastTick = millis();
        pet.tick();
        if (inv.bond < 1000) inv.bond++;
        saveAll(pet, inv);
        Serial.printf("[TICK] age:%d energy:%d morale:%d shield:%d bond:%d\n",
            pet.age, pet.hunger, pet.happiness, pet.health, inv.bond);
        // Refresh live data on Back screen; other modes need no stat update
        if (uiMode == UIMode::Back) {
            displayBackContent(pet, inv, nasaCargo);
            displayMenuBar(uiMode, actSel);
        }
    }

    // ── Character animation (Main mode only) ────────────────────────────
    if (uiMode == UIMode::Main) {
        charAnimUpdate(pet.age, CHAR_CX, CHAR_CY);
    }

    delay(16);
}
