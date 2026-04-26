#include "game_data.h"
#include <Preferences.h>

const ItemDef ITEM_DEFS[ITEM_COUNT] = {
    {"Apple",   3,  "+20 Hunger",  20,  0,   0 },
    {"Ball",    5,  "+15 Happy",    0, 15,   0 },
    {"Bandage", 8,  "+30 Health",   0,  0,  30 },
    {"Crown",  20,  "+5 All",       5,  5,   5 },
};

void applyItem(Pet& pet, int idx) {
    pet.hunger    = min(100, (int)pet.hunger    + ITEM_DEFS[idx].hungerBoost);
    pet.happiness = min(100, (int)pet.happiness + ITEM_DEFS[idx].happyBoost);
    pet.health    = min(100, (int)pet.health    + ITEM_DEFS[idx].healthBoost);
    pet.mood      = pet.calcMood();
}

void saveAll(const Pet& pet, const Inventory& inv) {
    Preferences p;
    p.begin("fido", false);
    p.putUChar("hunger", pet.hunger);
    p.putUChar("happy",  pet.happiness);
    p.putUChar("health", pet.health);
    p.putUChar("age",    pet.age);
    p.putUShort("coins", inv.coins);
    p.putUShort("bond",  inv.bond);
    for (int i = 0; i < ITEM_COUNT; i++) {
        char key[8];
        snprintf(key, sizeof(key), "it%d", i);
        p.putBool(key, inv.owned[i]);
    }
    p.end();
}

void loadAll(Pet& pet, Inventory& inv) {
    Preferences p;
    p.begin("fido", true);
    pet.hunger    = p.getUChar("hunger", 80);
    pet.happiness = p.getUChar("happy",  80);
    pet.health    = p.getUChar("health", 100);
    pet.age       = p.getUChar("age",    0);
    inv.coins     = p.getUShort("coins", 0);
    inv.bond      = p.getUShort("bond",  0);
    for (int i = 0; i < ITEM_COUNT; i++) {
        char key[8];
        snprintf(key, sizeof(key), "it%d", i);
        inv.owned[i] = p.getBool(key, false);
    }
    p.end();
    pet.mood = pet.calcMood();
}
