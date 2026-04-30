#pragma once
#include <Arduino.h>

#define NASA_CARGO_MAX 8

struct NasaCargo {
    char    items[NASA_CARGO_MAX][24]; // APOD title, max 23 chars + null
    uint8_t count;
};

// Fetch today's NASA APOD title via WiFi and append to cargo.
// Returns the title on success, "" on WiFi failure, "Cargo full!" when full.
String runNasaGacha(NasaCargo& cargo);

void saveNasaCargo(const NasaCargo& cargo);
void loadNasaCargo(NasaCargo& cargo);
