#pragma once
#include "pet.h"

enum class UIMode { Status, Action };

void displayLeftPanel(const Pet& pet, UIMode mode, int selection);
void displayMessage(const String& msg);
void displayMenu(UIMode mode, int selection);
