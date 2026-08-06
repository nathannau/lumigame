#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <TCA9555.h>
#include <FastLED.h>
#include <driver/gpio.h>
#include <initializer_list>

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

struct GameEntry {
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
extern GameEntry* currentGameEntry;

// Implemented by the menu's own blocks (a "to do" function named menu_loop()).
extern void menu_loop();

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

void addGame(void (*startFn)(), uint8_t (*loopFn)(), void (*stopFn)());

uint32_t getButtonsState();
bool getButtonState(uint8_t position);

void setAllLeds(CRGB color);
void setLedColor(uint8_t position, CRGB color);

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

// Named timers, identified by a free-form name (e.g. "attack", "blink").
#define MAX_TIMERS 16

struct TimerEntry {
  String name;
  unsigned long expireAt;
  bool active;
};

extern TimerEntry timerRegistry[MAX_TIMERS];

// (Re)starts the named timer so it expires `ms` milliseconds from now.
// Creates the timer on first use; silently does nothing if all MAX_TIMERS
// slots are already taken by other names.
void lumigameTimerSet(const String& name, unsigned long ms);

// True once the named timer's delay has elapsed. Also true if that timer
// was never started, so a first check can kick off a "set timer, then act"
// cycle without a separate "has it started" test.
bool lumigameTimerExpired(const String& name);

// Scrolls `text` across the LED grid using a tiny 4-row bitmap font (digits
// and A-Z; any other character, including space, renders as a blank gap).
// Call every loop() iteration with the same text/colour/speed - it tracks
// its own scroll position over time via millis(), so it doesn't block.
// Returns true once the text has scrolled fully off-screen; the position
// then restarts from the beginning on the next call, so a game can just
// keep calling this every frame to loop the message, or stop once it sees
// `true` to show it only once.
bool lumigameScrollText(const String& text, CRGB color, unsigned long stepMs);

// Stops any text currently scrolling and clears the LED grid immediately.
void lumigameStopScrollText();

// Whether lumigameScrollText() is currently displaying something (i.e. has
// been called with non-empty text and not stopped since).
bool lumigameIsScrollingText();
