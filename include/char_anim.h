#pragma once
#include <Arduino.h>

// age: pet.age (increments every 30s). Used to select the growth stage sprite.
void charAnimUpdate(uint8_t age);

// Force a full redraw of the character area (call after screen clear).
void charAnimRedraw();
