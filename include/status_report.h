#pragma once
#include "pet.h"
#include "game_data.h"

// Full-screen pet "health card". Blocks until any button is pressed.
void showStatusReport(const Pet& pet, const Inventory& inv);

// Confirmation dialog. Returns true if user confirmed (A), false if cancelled (C).
bool showConfirmDialog(const char* line1, const char* line2, const char* confirmLabel);
