#pragma once

#include "lumigame.h"

// How fast a game's name scrolls (ms per column) when previewed in the menu.
#define MENU_SCROLL_SPEED_MS 200

// Runs while no game is active: lights one LED per registered game, scrolls
// the name of a game when its button is clicked, and launches it (via
// startGame()) on a second click of the same button.
void menu_loop();
