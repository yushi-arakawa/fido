#pragma once
#include "pet.h"
#include "game_data.h"

enum class UIMode { Status, Action };

void displayLeftPanel(const Pet& pet, UIMode mode, int selection, const Inventory& inv);
void displayMessage(const String& msg);
void displayMenu(UIMode mode, int selection);
