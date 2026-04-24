#pragma once
#include <Arduino.h>

enum class PetMood { Happy, Neutral, Hungry, Sleepy, Sick };

struct Pet {
    String name;
    uint8_t hunger;    // 0-100 (100 = full)
    uint8_t happiness; // 0-100
    uint8_t health;    // 0-100
    uint8_t age;       // days
    PetMood mood;

    void tick();       // called every game loop cycle
    PetMood calcMood() const;
};
