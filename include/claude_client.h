#pragma once
#include <Arduino.h>
#include "pet.h"

// Sends pet status to Claude API and returns a short response message.
// Returns empty string on failure.
String askClaude(const Pet& pet, const String& userMessage);
