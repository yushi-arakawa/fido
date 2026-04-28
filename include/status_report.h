#pragma once
#include "pet.h"
#include "game_data.h"

// Settings screen: volume + data clear + power off. Blocks until closed.
void showSettings(Pet& pet, Inventory& inv);

// Confirmation dialog. Returns true if A pressed, false if C pressed.
bool showConfirmDialog(const char* line1, const char* line2, const char* confirmLabel);
