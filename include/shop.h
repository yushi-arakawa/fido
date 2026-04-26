#pragma once
#include "pet.h"
#include "game_data.h"

// Full-screen shop. Modifies pet stats, inv.coins, inv.owned in place.
void runShop(Pet& pet, Inventory& inv);
