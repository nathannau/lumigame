#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <TCA9555.h>
#include <FastLED.h>
#include <driver/gpio.h>
#include <initializer_list>
#include <string.h>

// Grid dimensions - buttons and LEDs share one flat "position" index (0-31)
// = row * NUM_COLS + column.
#define NUM_ROWS 4
#define NUM_COLS 8
#define NUM_BUTTONS (NUM_ROWS * NUM_COLS)
#define MAX_GAMES 32

// LED strip data pins, one per row (see README: 4 strips of 8). Named via
// the gpio_num_t constants from the framework's driver/gpio.h.
// GPIO18/19 (VSPI SCK/MISO) and GPIO23/5 (VSPI MOSI/SS) are deliberately left
// free for a future SPI peripheral (SD card, external RAM, MAX98357, ...).
#define LED_DATA_PIN_ROW0 GPIO_NUM_16
#define LED_DATA_PIN_ROW1 GPIO_NUM_17
#define LED_DATA_PIN_ROW2 GPIO_NUM_25
#define LED_DATA_PIN_ROW3 GPIO_NUM_26

// How long each colour is shown per LED during the boot diagnostic, in ms.
#define DIAGNOSTIC_STEP_MS 200

// A button's raw reading must stay stable for this long before it's
// accepted into buttonState (mechanical debounce).
#define BUTTON_DEBOUNCE_MS 20

// Flip which physical column scrolling text lands on (left<->right), to
// match the real wiring - flip this if scrolling text/glyphs appear
// mirrored on the actual hardware. Does not affect anything else (buttons,
// setAllLeds/setLedColor, the diagnostic, ...).
#define SCROLL_MIRROR_COLUMNS true

struct GameEntry
{
  char label[24];
  void (*start)();
  uint8_t (*loop)();
  void (*stop)();
};

extern TCA9555 tca1;
extern TCA9555 tca2;
extern CRGB leds[NUM_BUTTONS];
extern uint32_t buttonState;
extern GameEntry gameRegistry[MAX_GAMES];
extern uint8_t gameCount;
extern GameEntry *currentGameEntry;

// Called once from setup().
void lumigameInit();

// Lights each of the 32 LEDs one at a time in white (checks every position
// and wiring), then all LEDs together in red, green, and blue (checks every
// colour channel) - DIAGNOSTIC_STEP_MS per step. Blocking; takes
// (NUM_BUTTONS + 3) * DIAGNOSTIC_STEP_MS in total.
void lumigameDiagnostic();

// Called once per loop() iteration: refreshes the button state, runs the
// active game (or the menu when none is active), then pushes the LED
// buffer to the hardware.
void lumigameLoop();

void addGame(const char *label, void (*startFn)(), uint8_t (*loopFn)(), void (*stopFn)());

// Number of registered games, and the menu label given to game `index`
// (0..getGameCount()-1) when it was registered.
uint8_t getGameCount();
const char *getGameLabel(uint8_t index);

// Runs game `index`'s start() once and makes it the active game, so
// lumigameLoop() calls its loop()/stop() from the next iteration on.
void startGame(uint8_t index);

uint32_t getButtonsState();
bool getButtonState(uint8_t position);

// True only on the one lumigameLoop() iteration where the button's
// (debounced) state changes to pressed/released, respectively.
bool getButtonJustPressed(uint8_t position);
bool getButtonJustReleased(uint8_t position);

// Bitmask versions of the above (bit = position), e.g. for the menu's own
// click detection.
uint32_t getButtonsJustPressed();
uint32_t getButtonsJustReleased();

// What lumigame_button_state's dropdown selects.
enum ButtonStateQuery
{
  BUTTON_PRESSED,
  BUTTON_RELEASED,
  BUTTON_JUST_PRESSED,
  BUTTON_JUST_RELEASED,
};

// Whether the button at `position` currently matches `query`.
bool queryButtonState(uint8_t position, ButtonStateQuery query);

void setAllLeds(CRGB color);
void setLedColor(uint8_t position, CRGB color);

// Scales all three channels of `color` by `brightness`/255: 0 brings every
// channel to 0, 255 leaves the colour unchanged.
CRGB lumigameDimColor(CRGB color, uint8_t brightness);

// Adds two positions, wrapping the row and column independently so the
// result always stays within the 4x8 grid.
uint8_t lumigamePositionAdd(uint8_t a, uint8_t b);

// Fills out[0..NUM_BUTTONS) with every position (0-31) in a random order.
void shuffledPositions(uint8_t out[NUM_BUTTONS]);

// A "zone" is a subset of the 32 button positions, packed as one bit per
// position (bit N = position N). Built from a 4x8 bitmap field's flattened
// row-major values.
uint32_t lumigameZoneMask(std::initializer_list<uint8_t> bits);

// Whether the given position belongs to the zone.
bool lumigamePositionInZone(uint8_t position, uint32_t zone);

// Timers, indexed 0..MAX_TIMERS-1 (see the "Minuteurs" blocks).
#define MAX_TIMERS 16

// (Re)starts timer `index` so it expires `ms` milliseconds from now. No-op
// if `index` is out of range.
void lumigameTimerSet(uint8_t index, unsigned long ms);

// True once timer `index`'s delay has elapsed. Also true if that timer was
// never started (or `index` is out of range), so a first check can kick off
// a "set timer, then act" cycle without a separate "has it started" test.
bool lumigameTimerExpired(uint8_t index);

// Milliseconds left before timer `index` expires. 0 if it has already
// expired, was never started, or `index` is out of range.
unsigned long lumigameTimerRemaining(uint8_t index);

// Scrolls `text` across the LED grid using a tiny 4-row bitmap font (digits
// and A-Z; any other character, including space, renders as a blank gap).
// Call every loop() iteration with the same text/colour/speed - it tracks
// its own scroll position over time via millis(), so it doesn't block.
// An empty `text` means "keep whatever is already scrolling" - pass "" once
// the text has been set so the caller doesn't have to recompute/reconnect
// it every frame; use lumigameStopScrollText() to actually clear it early.
// Returns true once the text has scrolled fully off-screen - at that point
// it stops on its own (lumigameIsScrollingText() becomes false); call this
// again with the actual text (not "") to play it again/loop it.
bool lumigameScrollText(const String &text, CRGB color, unsigned long stepMs);

// The return value of the most recent lumigameScrollText() call: true if
// that call was the one that completed a full scroll pass. Check this
// right after calling lumigame_scroll_text (in the same frame) to react
// to "the text just finished scrolling" without needing the statement
// block itself to carry a value.
bool lumigameScrollTextFinished();

// Stops any text currently scrolling and clears the LED grid immediately.
void lumigameStopScrollText();

// Whether lumigameScrollText() is still mid a scroll pass (has been called
// with non-empty text and hasn't finished or been stopped since).
bool lumigameIsScrollingText();
