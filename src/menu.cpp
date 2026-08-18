#include "menu.h"

static uint8_t menuPreviewIndex = 0xFF; // 0xFF = no game currently previewed
static bool menuShowingName = false; // true while the previewed game's name is scrolling

void menu_loop()
{
  uint32_t justPressed = getButtonsJustPressed();

  // Second click on the previewed game's button -> launch it.
  if (menuPreviewIndex != 0xFF && ((justPressed >> menuPreviewIndex) & 0x01))
  {
    uint8_t index = menuPreviewIndex;
    menuPreviewIndex = 0xFF;
    menuShowingName = false;
    lumigameStopScrollText();
    startGame(index);
    return;
  }

  // First click on a (different) game's button -> (re)start previewing it.
  bool justSelected = false;
  for (uint8_t i = 0; i < gameCount; i++)
  {
    if ((justPressed >> i) & 0x01)
    {
      menuPreviewIndex = i;
      menuShowingName = true;
      justSelected = true;
      break;
    }
  }

  if (menuShowingName)
  {
    // Only pass the actual label on the frame it's (re)selected - "" on
    // every other frame keeps scrolling it without recomputing/reconnecting it.
    const char *label = justSelected ? getGameLabel(menuPreviewIndex) : "";
    if (lumigameScrollText(label, CRGB::White, MENU_SCROLL_SPEED_MS))
    {
      // Finished one full pass -> go back to showing the button grid.
      menuShowingName = false;
    }
  }
  else
  {
    // Available games in white, the previewed one (if any) in blue.
    setAllLeds(CRGB::Black);
    for (uint8_t i = 0; i < gameCount; i++)
    {
      setLedColor(i, (i == menuPreviewIndex) ? CRGB::Blue : CRGB::White);
    }
  }
}
