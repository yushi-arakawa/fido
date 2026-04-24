#pragma once

// Draws the planet sprite with bob + sparkle animation.
// Call once per frame from the main loop.
void charAnimUpdate();

// Force a full redraw of the character area (call after screen clear).
void charAnimRedraw();
