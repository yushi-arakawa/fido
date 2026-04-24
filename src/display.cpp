#include "display.h"
#include <M5Stack.h>

static const char* moodLabel(PetMood m) {
    switch (m) {
        case PetMood::Happy:   return "Happy   :D";
        case PetMood::Hungry:  return "Hungry  :(";
        case PetMood::Sleepy:  return "Sleepy  zz";
        case PetMood::Sick:    return "Sick    xx";
        default:               return "Neutral  .";
    }
}

void displayPet(const Pet& pet) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.printf("%s  Day %d", pet.name.c_str(), pet.age);

    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.printf("Hunger   : %3d", pet.hunger);
    M5.Lcd.setCursor(10, 70);
    M5.Lcd.printf("Happiness: %3d", pet.happiness);
    M5.Lcd.setCursor(10, 90);
    M5.Lcd.printf("Health   : %3d", pet.health);
    M5.Lcd.setCursor(10, 110);
    M5.Lcd.printf("Mood     : %s", moodLabel(pet.mood));
}

void displayMessage(const String& msg) {
    M5.Lcd.fillRect(0, 140, 320, 100, DARKGREY);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(5, 145);
    M5.Lcd.setTextColor(WHITE, DARKGREY);
    M5.Lcd.println(msg);
    M5.Lcd.setTextColor(WHITE, BLACK);
}

void displayMenu() {
    M5.Lcd.setTextSize(1);
    // Button labels at the bottom (A=left, B=center, C=right)
    M5.Lcd.setCursor(10,  220); M5.Lcd.print("[Feed]");
    M5.Lcd.setCursor(130, 220); M5.Lcd.print("[Play]");
    M5.Lcd.setCursor(250, 220); M5.Lcd.print("[Talk]");
}
