#include "lumigame.h"
#include "menu.h"

TCA9555 tca1(0x20);
TCA9555 tca2(0x21);
CRGB leds[NUM_BUTTONS];
uint32_t buttonState = 0;
GameEntry gameRegistry[MAX_GAMES];
uint8_t gameCount = 0;
GameEntry *currentGameEntry = nullptr;

void lumigameInit()
{
  Wire.begin();
  tca1.begin();
  tca2.begin();
  // TODO: confirm LED chipset (assuming WS2812B/GRB) and data pin order match the wiring.
  FastLED.addLeds<WS2812B, LED_DATA_PIN_ROW0, GRB>(leds + 0 * NUM_COLS, NUM_COLS);
  FastLED.addLeds<WS2812B, LED_DATA_PIN_ROW1, GRB>(leds + 1 * NUM_COLS, NUM_COLS);
  FastLED.addLeds<WS2812B, LED_DATA_PIN_ROW2, GRB>(leds + 2 * NUM_COLS, NUM_COLS);
  FastLED.addLeds<WS2812B, LED_DATA_PIN_ROW3, GRB>(leds + 3 * NUM_COLS, NUM_COLS);

  lumigameDiagnostic();
}

void lumigameDiagnostic()
{
  for (uint8_t pos = 0; pos < NUM_BUTTONS; pos++)
  {
    setAllLeds(CRGB::Black);
    setLedColor(pos, CRGB(255, 255, 255));
    FastLED.show();
    delay(DIAGNOSTIC_STEP_MS);
  }

  const CRGB colours[3] = {CRGB(255, 0, 0), CRGB(0, 255, 0), CRGB(0, 0, 255)};
  for (uint8_t c = 0; c < 3; c++)
  {
    setAllLeds(colours[c]);
    FastLED.show();
    delay(DIAGNOSTIC_STEP_MS * 3);
  }

  setAllLeds(CRGB::Black);
  FastLED.show();
}

static uint32_t rawButtonState = 0;
static unsigned long buttonChangedAt[NUM_BUTTONS] = {0};
static uint32_t buttonJustPressedMask = 0;
static uint32_t buttonJustReleasedMask = 0;

// Debounces the raw TCA9555 reading into `buttonState`: a bit is only
// committed once its raw value has stayed unchanged for BUTTON_DEBOUNCE_MS,
// timed independently per button so one bouncing button doesn't delay the
// others. Also derives the "just pressed"/"just released" edge masks from
// the debounced state, once per frame, for every consumer to share.
static void updateButtonState()
{
  // Buttons are wired with pull-ups: a pressed button pulls its pin LOW.
  // Invert here, once, so 1 means "pressed" for every consumer
  // (getButtonState(), the menu's click detection, etc.).
  uint32_t raw = ~(((uint32_t)tca2.read16() << 16) | tca1.read16());
  uint32_t changedBits = raw ^ rawButtonState;
  unsigned long now = millis();

  for (uint8_t i = 0; i < NUM_BUTTONS; i++)
  {
    if ((changedBits >> i) & 0x01)
    {
      buttonChangedAt[i] = now;
    }
  }
  rawButtonState = raw;

  uint32_t previousButtonState = buttonState;
  for (uint8_t i = 0; i < NUM_BUTTONS; i++)
  {
    if (now - buttonChangedAt[i] >= BUTTON_DEBOUNCE_MS)
    {
      uint32_t bit = (uint32_t)1 << i;
      if (raw & bit)
        buttonState |= bit;
      else
        buttonState &= ~bit;
    }
  }

  buttonJustPressedMask = buttonState & ~previousButtonState;
  buttonJustReleasedMask = previousButtonState & ~buttonState;
}

void lumigameLoop()
{
  updateButtonState();
  if (currentGameEntry)
  {
    if (currentGameEntry->loop())
    {
      currentGameEntry->stop();
      currentGameEntry = nullptr;
    }
  }
  else
  {
    menu_loop();
  }
  FastLED.show();
}

void addGame(const char *label, void (*startFn)(), uint8_t (*loopFn)(), void (*stopFn)())
{
  if (gameCount < MAX_GAMES)
  {
    strncpy(gameRegistry[gameCount].label, label, sizeof(gameRegistry[gameCount].label) - 1);
    gameRegistry[gameCount].label[sizeof(gameRegistry[gameCount].label) - 1] = '\0';
    gameRegistry[gameCount].start = startFn;
    gameRegistry[gameCount].loop = loopFn;
    gameRegistry[gameCount].stop = stopFn;
    gameCount++;
  }
}

uint8_t getGameCount()
{
  return gameCount;
}

const char *getGameLabel(uint8_t index)
{
  if (index >= gameCount)
    return "";
  return gameRegistry[index].label;
}

void startGame(uint8_t index)
{
  if (index >= gameCount)
    return;
  currentGameEntry = &gameRegistry[index];
  currentGameEntry->start();
}

uint32_t getButtonsState()
{
  return buttonState;
}

bool getButtonState(uint8_t position)
{
  if (position >= NUM_BUTTONS)
    return false;
  return (buttonState >> position) & 0x01;
}

uint32_t getButtonsJustPressed()
{
  return buttonJustPressedMask;
}

uint32_t getButtonsJustReleased()
{
  return buttonJustReleasedMask;
}

bool getButtonJustPressed(uint8_t position)
{
  if (position >= NUM_BUTTONS)
    return false;
  return (buttonJustPressedMask >> position) & 0x01;
}

bool getButtonJustReleased(uint8_t position)
{
  if (position >= NUM_BUTTONS)
    return false;
  return (buttonJustReleasedMask >> position) & 0x01;
}

bool queryButtonState(uint8_t position, ButtonStateQuery query)
{
  switch (query)
  {
  case BUTTON_PRESSED:
    return getButtonState(position);
  case BUTTON_RELEASED:
    return !getButtonState(position);
  case BUTTON_JUST_PRESSED:
    return getButtonJustPressed(position);
  case BUTTON_JUST_RELEASED:
    return getButtonJustReleased(position);
  }
  return false;
}

void setAllLeds(CRGB color)
{
  for (uint8_t i = 0; i < NUM_BUTTONS; i++)
    leds[i] = color;
}

void setLedColor(uint8_t position, CRGB color)
{
  if (position >= NUM_BUTTONS)
    return;
  leds[position] = color;
}

CRGB lumigameDimColor(CRGB color, uint8_t brightness)
{
  return CRGB(color.r * brightness / 255, color.g * brightness / 255, color.b * brightness / 255);
}

uint8_t lumigamePositionAdd(uint8_t a, uint8_t b)
{
  uint8_t row = ((a / NUM_COLS) + (b / NUM_COLS)) % NUM_ROWS;
  uint8_t col = ((a % NUM_COLS) + (b % NUM_COLS)) % NUM_COLS;
  return row * NUM_COLS + col;
}

void shuffledPositions(uint8_t out[NUM_BUTTONS])
{
  for (uint8_t i = 0; i < NUM_BUTTONS; i++)
    out[i] = i;
  for (uint8_t i = NUM_BUTTONS - 1; i > 0; i--)
  {
    uint8_t j = random(i + 1);
    uint8_t tmp = out[i];
    out[i] = out[j];
    out[j] = tmp;
  }
}

uint32_t lumigameZoneMask(std::initializer_list<uint8_t> bits)
{
  uint32_t mask = 0;
  uint8_t i = 0;
  for (uint8_t b : bits)
  {
    if (b)
      mask |= ((uint32_t)1 << i);
    i++;
  }
  return mask;
}

bool lumigamePositionInZone(uint8_t position, uint32_t zone)
{
  if (position >= NUM_BUTTONS)
    return false;
  return (zone >> position) & 0x01;
}

// expireAt == 0 (the static-init default) reads as already-expired via the
// same millis() comparison used once a timer is actually set — so an unset
// timer needs no separate "active" flag to report "expired" by default.
static unsigned long timerExpireAt[MAX_TIMERS];

void lumigameTimerSet(uint8_t index, unsigned long ms)
{
  if (index < MAX_TIMERS)
    timerExpireAt[index] = millis() + ms;
}

bool lumigameTimerExpired(uint8_t index)
{
  if (index >= MAX_TIMERS)
    return true;
  return (long)(millis() - timerExpireAt[index]) >= 0;
}

unsigned long lumigameTimerRemaining(uint8_t index)
{
  if (index >= MAX_TIMERS)
    return 0;
  long remaining = (long)(timerExpireAt[index] - millis());
  return remaining > 0 ? (unsigned long)remaining : 0;
}

// Tiny 4x5 font (4 physical rows wide, 5 steps "tall"), digits 0-9 then A-Z.
// The display only has 4 physical rows, so glyphs are authored rotated 90°:
// each glyph is drawn like a normal small font (5 rows top-to-bottom, 4
// columns left-to-right, '#'=1/'.'=0 converted straight to a binary
// literal), then scrolled through the 8 physical columns exactly like the
// old 4x3 font was - only the per-character step count changed (3 -> 5),
// giving each letter much more shape detail. Bit order per row: leftmost
// character = physical row 0, rightmost = physical row 3.
#define FONT_GLYPH_HEIGHT 5
#define FONT_CHAR_SPACING 1

static const uint8_t FONT_GLYPHS[36][FONT_GLYPH_HEIGHT] = {
    /* 0 */ {0b0110, 0b1001, 0b1001, 0b1001, 0b0110},
    /* 1 */ {0b0010, 0b0110, 0b0010, 0b0010, 0b0111},
    /* 2 */ {0b0110, 0b1001, 0b0010, 0b0100, 0b1111},
    /* 3 */ {0b0110, 0b0001, 0b0110, 0b0001, 0b0110},
    /* 4 */ {0b0010, 0b0110, 0b1010, 0b1111, 0b0010},
    /* 5 */ {0b1111, 0b1000, 0b1110, 0b0001, 0b1110},
    /* 6 */ {0b0110, 0b1000, 0b1110, 0b1001, 0b0110},
    /* 7 */ {0b1111, 0b0001, 0b0010, 0b0100, 0b0100},
    /* 8 */ {0b0110, 0b1001, 0b0110, 0b1001, 0b0110},
    /* 9 */ {0b0110, 0b1001, 0b0111, 0b0001, 0b0110},
    /* A */ {0b0110, 0b1001, 0b1111, 0b1001, 0b1001},
    /* B */ {0b1110, 0b1001, 0b1110, 0b1001, 0b1110},
    /* C */ {0b0110, 0b1001, 0b1000, 0b1001, 0b0110},
    /* D */ {0b1110, 0b1001, 0b1001, 0b1001, 0b1110},
    /* E */ {0b1111, 0b1000, 0b1110, 0b1000, 0b1111},
    /* F */ {0b1111, 0b1000, 0b1110, 0b1000, 0b1000},
    /* G */ {0b0110, 0b1000, 0b1011, 0b1001, 0b0110},
    /* H */ {0b1001, 0b1001, 0b1111, 0b1001, 0b1001},
    /* I */ {0b1110, 0b0100, 0b0100, 0b0100, 0b1110},
    /* J */ {0b0011, 0b0001, 0b0001, 0b1001, 0b0110},
    /* K */ {0b1001, 0b1010, 0b1100, 0b1010, 0b1001},
    /* L */ {0b1000, 0b1000, 0b1000, 0b1000, 0b1111},
    /* M */ {0b1001, 0b1111, 0b1001, 0b1001, 0b1001},
    /* N */ {0b1001, 0b1101, 0b1011, 0b1001, 0b1001},
    /* O */ {0b0110, 0b1001, 0b1001, 0b1001, 0b0110},
    /* P */ {0b1110, 0b1001, 0b1110, 0b1000, 0b1000},
    /* Q */ {0b0110, 0b1001, 0b1001, 0b0110, 0b0001},
    /* R */ {0b1110, 0b1001, 0b1110, 0b1010, 0b1001},
    /* S */ {0b0111, 0b1000, 0b0110, 0b0001, 0b1110},
    /* T */ {0b1111, 0b0100, 0b0100, 0b0100, 0b0100},
    /* U */ {0b1001, 0b1001, 0b1001, 0b1001, 0b0110},
    /* V */ {0b1001, 0b1001, 0b1001, 0b0110, 0b0110},
    /* W */ {0b1001, 0b1001, 0b1111, 0b1111, 0b1001},
    /* X */ {0b1001, 0b0110, 0b0110, 0b0110, 0b1001},
    /* Y */ {0b1001, 0b1001, 0b0110, 0b0100, 0b0100},
    /* Z */ {0b1111, 0b0001, 0b0110, 0b1000, 0b1111},
};

// Returns nullptr for unsupported characters (including space), which the
// caller renders as a blank gap.
static const uint8_t *lumigameFontGlyph(char c)
{
  if (c >= 'a' && c <= 'z')
    c -= 'a' - 'A';
  if (c >= '0' && c <= '9')
    return FONT_GLYPHS[c - '0'];
  if (c >= 'A' && c <= 'Z')
    return FONT_GLYPHS[10 + (c - 'A')];
  return nullptr;
}

// Whether the pixel at (absolute text column, row) is lit. `col` runs from
// 0 (first column of the first character) across the whole string, made up
// of FONT_GLYPH_HEIGHT glyph steps plus FONT_CHAR_SPACING blank steps per
// character.
static bool lumigameFontPixelAt(const String &text, uint16_t col, uint8_t row)
{
  const uint16_t charAdvance = FONT_GLYPH_HEIGHT + FONT_CHAR_SPACING;
  uint16_t charIndex = col / charAdvance;
  uint16_t colInChar = col % charAdvance;
  if (charIndex >= text.length() || colInChar >= FONT_GLYPH_HEIGHT)
    return false;
  const uint8_t *glyph = lumigameFontGlyph(text[charIndex]);
  if (!glyph)
    return false;
  return (glyph[colInChar] >> (NUM_ROWS - 1 - row)) & 0x01;
}

static String scrollActiveText;
static uint16_t scrollOffset = 0;
static unsigned long scrollLastStepAt = 0;
static bool scrollJustFinished = false;

bool lumigameScrollText(const String &text, CRGB color, unsigned long stepMs)
{
  // An empty text means "keep whatever is already scrolling" - lets a
  // caller pass "" every frame instead of recomputing/reconnecting the
  // text expression once it has already been set. Use
  // lumigameStopScrollText() to actually clear the display.
  if (text.length() > 0 && text != scrollActiveText)
  {
    scrollActiveText = text;
    scrollOffset = 0;
    scrollLastStepAt = millis();
  }

  const uint16_t charAdvance = FONT_GLYPH_HEIGHT + FONT_CHAR_SPACING;
  uint16_t textWidth = scrollActiveText.length() * charAdvance;
  uint16_t totalSteps = textWidth + NUM_COLS;

  bool finished = false;
  if (millis() - scrollLastStepAt >= stepMs)
  {
    scrollLastStepAt += stepMs;
    scrollOffset++;
    if (scrollOffset >= totalSteps)
    {
      scrollOffset = 0;
      finished = true;
      // Stop here rather than silently looping forever: lumigameIsScrollingText()
      // should reflect "still mid a pass", not "would resume if called again".
      // A caller that wants to keep looping just calls this again with the
      // text (or "") on the next frame, which restarts it fresh.
      scrollActiveText = "";
    }
  }

  setAllLeds(CRGB::Black);
  for (uint8_t dc = 0; dc < NUM_COLS; dc++)
  {
    int32_t textCol = (int32_t)dc + scrollOffset - NUM_COLS;
    if (textCol < 0 || textCol >= (int32_t)textWidth)
      continue;
    uint8_t physCol = SCROLL_MIRROR_COLUMNS ? (NUM_COLS - 1 - dc) : dc;
    for (uint8_t row = 0; row < NUM_ROWS; row++)
    {
      if (lumigameFontPixelAt(scrollActiveText, (uint16_t)textCol, row))
      {
        setLedColor(row * NUM_COLS + physCol, color);
      }
    }
  }

  scrollJustFinished = finished;
  return finished;
}

bool lumigameScrollTextFinished()
{
  return scrollJustFinished;
}

void lumigameStopScrollText()
{
  scrollActiveText = "";
  scrollOffset = 0;
  setAllLeds(CRGB::Black);
}

bool lumigameIsScrollingText()
{
  return scrollActiveText.length() > 0;
}
