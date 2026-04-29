#pragma once
#include "pet.h"
#include "game_data.h"
#include "nasa_gacha.h"

enum class UIMode { Main, Act, Back };

// Full redraw including background. Call on mode switch or after returning from fullscreen.
void displayInit(UIMode mode, const Pet& pet, const Inventory& inv, const NasaCargo& nasa, int sel);

// Lightweight content-only redraws (no background redraw — noise-safe):
void displayActContent(int sel);
void displayBackContent(const Pet& pet, const Inventory& inv, const NasaCargo& nasa);

// Update message box only (Main + Act modes).
void displayMessage(const String& msg);

// Redraw bottom button bar only.
void displayMenuBar(UIMode mode, int sel);
