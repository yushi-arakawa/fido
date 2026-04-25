#include <M5Stack.h>
#include "pet.h"
#include "display.h"
#include "claude_client.h"
#include "char_anim.h"

static Pet    pet       = {"Fido", 80, 80, 100, 0, PetMood::Happy};
static UIMode uiMode    = UIMode::Status;
static int    actionIdx = 0;

static unsigned long lastTick = 0;
static const unsigned long TICK_MS = 30000;

static void doAction(int idx) {
    switch (idx) {
        case 0: // Feed
            pet.hunger    = min(100, (int)pet.hunger    + 30);
            pet.happiness = min(100, (int)pet.happiness + 5);
            pet.mood      = pet.calcMood();
            displayLeftPanel(pet, uiMode, actionIdx);
            displayMessage("Yum! Nom nom nom...");
            break;
        case 1: // Play
            pet.happiness = min(100, (int)pet.happiness + 20);
            pet.hunger    = max(0,   (int)pet.hunger    - 10);
            pet.mood      = pet.calcMood();
            displayLeftPanel(pet, uiMode, actionIdx);
            displayMessage("Wheee! So fun!");
            break;
        case 2: // Talk
            displayMessage("...");
            displayMessage(askClaude(pet, "How are you feeling?"));
            break;
    }
    displayMenu(uiMode, actionIdx);
}

void setup() {
    M5.begin();
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.fillScreen(BLACK);
    lastTick = millis();
    displayLeftPanel(pet, uiMode, actionIdx);
    displayMessage("Hello! I'm Fido!");
    displayMenu(uiMode, actionIdx);
}

void loop() {
    M5.update();

    // Right button: toggle Status / Action mode
    if (M5.BtnC.wasPressed()) {
        uiMode = (uiMode == UIMode::Status) ? UIMode::Action : UIMode::Status;
        displayLeftPanel(pet, uiMode, actionIdx);
        displayMenu(uiMode, actionIdx);
    }

    // Left button: move selection (Action mode only)
    if (M5.BtnA.wasPressed() && uiMode == UIMode::Action) {
        actionIdx = (actionIdx + 1) % 3;
        displayLeftPanel(pet, uiMode, actionIdx);
        displayMenu(uiMode, actionIdx);
    }

    // Middle button: confirm (Action mode only)
    if (M5.BtnB.wasPressed() && uiMode == UIMode::Action) {
        doAction(actionIdx);
    }

    // Game tick
    if (millis() - lastTick >= TICK_MS) {
        lastTick = millis();
        pet.tick();
        displayLeftPanel(pet, uiMode, actionIdx);
        displayMenu(uiMode, actionIdx);
    }

    charAnimUpdate();
    delay(16);
}
