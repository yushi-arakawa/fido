#pragma once
#include <Arduino.h>

// Draw/animate the pet sprite at screen position (cx, cy).
// Only call this in Main mode; not needed in Act/Back modes.
void charAnimUpdate(uint8_t age, int cx, int cy);

// Force full redraw on next charAnimUpdate call (call after screen clear).
void charAnimRedraw();
