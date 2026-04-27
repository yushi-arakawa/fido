#include "status_report.h"
#include "space_ui.h"
#include <M5Stack.h>

static int bondStars(uint16_t bond) { return min(5, (int)(bond / 200)); }

static const char* advisorMessage(const Pet& pet) {
    if (pet.health   < 30) return "Hull breach detected. Help me!";
    if (pet.hunger   < 20) return "Fuel critical. Feed me now!";
    if (pet.hunger   < 40) return "Running low on fuel...";
    if (pet.happiness < 25) return "Crew morale is very low.";
    if (pet.happiness < 50) return "Need some recreation time.";
    if (pet.health   < 50) return "Shields at half capacity.";
    if (pet.hunger > 80 && pet.happiness > 80 && pet.health > 80)
        return "All systems nominal. Warp ready!";
    if (pet.happiness > 70) return "Happy and exploring!";
    return "Systems stable. All is well.";
}

void showStatusReport(const Pet& pet, const Inventory& inv) {
    // Background drawn once
    spDrawBackground();
    spDrawStarfield(0, 0, 320, 240);

    // Header
    M5.Lcd.fillRect(0, 0, 320, 28, SM_HDR);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 6);
    M5.Lcd.printf("%.8s  Health Card", pet.name.c_str());
    M5.Lcd.drawFastHLine(0, 28, 320, SM_BORDER);

    // Bond row
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(12, 36);
    M5.Lcd.printf("Age: %-3d day(s)   Bond: ", pet.age);
    int stars = bondStars(inv.bond);
    M5.Lcd.setTextColor(SM_WHITE, SM_BG);
    for (int i = 0; i < 5; i++) M5.Lcd.print(i < stars ? "*" : "-");
    M5.Lcd.setTextColor(SM_DIM, SM_BG);
    M5.Lcd.printf(" (%d/1000)", inv.bond);
    M5.Lcd.drawFastHLine(0, 50, 320, SM_DIV);

    // Stat bars
    struct { const char* lbl; uint8_t val; } stats[3] = {
        {"Energy", pet.hunger},
        {"Morale", pet.happiness},
        {"Shield", pet.health},
    };
    for (int i = 0; i < 3; i++) {
        int y = 58 + i * 18;
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(SM_GREY, SM_BG);
        M5.Lcd.setCursor(12, y);
        M5.Lcd.printf("%-7s", stats[i].lbl);
        spBar(75, y, 160, 8, stats[i].val);
        M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
        M5.Lcd.setCursor(242, y);
        M5.Lcd.printf("%3d%%", stats[i].val);
    }
    M5.Lcd.drawFastHLine(0, 114, 320, SM_DIV);

    // Coins + items
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(12, 121);
    M5.Lcd.print("Coins: ");
    M5.Lcd.setTextColor(SM_WHITE, SM_BG);
    int ownedCount = 0;
    for (int i = 0; i < ITEM_COUNT; i++) if (inv.owned[i]) ownedCount++;
    M5.Lcd.printf("%-5d", inv.coins);
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.print("  Items: ");
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
    M5.Lcd.printf("%d/%d", ownedCount, ITEM_COUNT);
    M5.Lcd.drawFastHLine(0, 134, 320, SM_DIV);

    // Advisor
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(12, 142);
    M5.Lcd.print("Advisor:");
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
    M5.Lcd.setCursor(12, 156);
    M5.Lcd.println(advisorMessage(pet));

    // Footer
    spCornerFrame(0, 0, 320, 240);
    M5.Lcd.drawFastHLine(0, 210, 320, SM_DIV);
    M5.Lcd.setTextColor(SM_DIM, SM_BG);
    M5.Lcd.setCursor(70, 218);
    M5.Lcd.print("Press any button to close");

    while (true) {
        M5.update();
        if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed()) break;
        delay(20);
    }
}

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
        if (M5.BtnA.wasPressed()) return true;
        if (M5.BtnC.wasPressed()) return false;
        delay(20);
    }
}
