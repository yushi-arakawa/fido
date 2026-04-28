#include "pet.h"

PetMood Pet::calcMood() const {
    if (health < 30)    return PetMood::Sick;
    if (hunger < 20)    return PetMood::Hungry;
    if (happiness > 70) return PetMood::Happy;
    if (happiness < 30) return PetMood::Sleepy;
    return PetMood::Neutral;
}

void Pet::tick() {
    if (age < 255) age++;       // 1 tick = 30s → stage changes every 20 ticks (~10 min)
    if (hunger    > 0)  hunger    -= 2;
    if (happiness > 0)  happiness -= 1;
    if (hunger < 20)    health    = max(0, (int)health - 1);

    mood = calcMood();
}
