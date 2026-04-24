#include <M5Stack.h>
#include "pet.h"
#include "display.h"
#include "claude_client.h"
#include "char_anim.h"

static Pet pet = {"Fido", 80, 80, 100, 0, PetMood::Happy};

static unsigned long lastTick  = 0;
static const unsigned long TICK_MS = 30000; // 30 sec = 1 game cycle

static void handleTalk() {
    displayMessage("Asking Claude...");
    String reply = askClaude(pet, "How are you feeling right now?");
    if (reply.isEmpty()) reply = "(no response)";
    displayMessage(reply);
}

void setup() {
    M5.begin();
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.fillScreen(BLACK);
    lastTick = millis();
    displayPet(pet);
    displayMenu();
}

void loop() {
    M5.update();

    if (M5.BtnA.wasPressed()) {          // Feed
        pet.hunger    = min(100, (int)pet.hunger    + 30);
        pet.happiness = min(100, (int)pet.happiness + 5);
        pet.mood      = pet.calcMood();
        displayPet(pet);
        displayMessage("Yum! Nom nom nom...");
        displayMenu();
    }

    if (M5.BtnB.wasPressed()) {          // Play
        pet.happiness = min(100, (int)pet.happiness + 20);
        pet.hunger    = max(0,   (int)pet.hunger    - 10);
        pet.mood      = pet.calcMood();
        displayPet(pet);
        displayMessage("Wheee! So fun!");
        displayMenu();
    }

    if (M5.BtnC.wasPressed()) {          // Talk to Claude
        handleTalk();
        displayMenu();
    }

    // Game tick
    if (millis() - lastTick >= TICK_MS) {
        lastTick = millis();
        pet.tick();
        displayPet(pet);
        displayMenu();
    }

    // Character animation (~60fps target)
    charAnimUpdate();
    delay(16);
}
