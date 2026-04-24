#include "display.h"
#include "char_anim.h"
#include <M5Stack.h>

// Layout constants
static const int STAT_X   = 5;
static const int STAT_W   = 155;
static const int MSG_Y    = 155;
static const int MSG_H    = 58;
static const int MENU_Y   = 218;

static const char* moodLabel(PetMood m) {
    switch (m) {
        case PetMood::Happy:   return "Happy :D";
        case PetMood::Hungry:  return "Hungry:(";
        case PetMood::Sleepy:  return "Sleepy zz";
        case PetMood::Sick:    return "Sick  xx";
        default:               return "Neutral.";
    }
}

static uint16_t statBar(uint8_t val) {
    if (val > 66) return GREEN;
    if (val > 33) return YELLOW;
    return RED;
}

static void drawBar(int x, int y, uint8_t val) {
    int filled = map(val, 0, 100, 0, 80);
    M5.Lcd.fillRect(x,          y, filled,      6, statBar(val));
    M5.Lcd.fillRect(x + filled, y, 80 - filled, 6, DARKGREY);
}

void displayPet(const Pet& pet) {
    // Clear only the left stat panel — character panel is handled by charAnim
    M5.Lcd.fillRect(0, 0, STAT_W + 10, MSG_Y, BLACK);

    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(WHITE, BLACK);

    M5.Lcd.setCursor(STAT_X, 8);
    M5.Lcd.setTextSize(2);
    M5.Lcd.print(pet.name);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(STAT_X, 30);
    M5.Lcd.printf("Day %d", pet.age);

    M5.Lcd.setCursor(STAT_X, 55);  M5.Lcd.print("Hunger");
    drawBar(STAT_X, 65, pet.hunger);

    M5.Lcd.setCursor(STAT_X, 80);  M5.Lcd.print("Happy");
    drawBar(STAT_X, 90, pet.happiness);

    M5.Lcd.setCursor(STAT_X, 105); M5.Lcd.print("Health");
    drawBar(STAT_X, 115, pet.health);

    M5.Lcd.setCursor(STAT_X, 135);
    M5.Lcd.printf("%-10s", moodLabel(pet.mood));

    charAnimRedraw();
}

void displayMessage(const String& msg) {
    M5.Lcd.fillRect(0, MSG_Y, 320, MSG_H, DARKGREY);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(WHITE, DARKGREY);
    M5.Lcd.setCursor(5, MSG_Y + 4);
    M5.Lcd.println(msg);
    M5.Lcd.setTextColor(WHITE, BLACK);
}

void displayMenu() {
    M5.Lcd.fillRect(0, MENU_Y, 320, 22, BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(10,  MENU_Y + 4); M5.Lcd.print("[Feed]");
    M5.Lcd.setCursor(130, MENU_Y + 4); M5.Lcd.print("[Play]");
    M5.Lcd.setCursor(250, MENU_Y + 4); M5.Lcd.print("[Talk]");
}
