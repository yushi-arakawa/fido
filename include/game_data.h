#pragma once
#include <Arduino.h>
#include "pet.h"

static const int ITEM_COUNT = 4;

struct ItemDef {
    const char* name;
    uint16_t    cost;
    const char* desc;
    int8_t      hungerBoost;
    int8_t      happyBoost;
    int8_t      healthBoost;
};

extern const ItemDef ITEM_DEFS[ITEM_COUNT];

struct Inventory {
    uint16_t coins;
    uint16_t bond;           // 絆レベル (0-1000)
    bool     owned[ITEM_COUNT];
};

void saveAll(const Pet& pet, const Inventory& inv);
void loadAll(Pet& pet, Inventory& inv);
void applyItem(Pet& pet, int idx);
