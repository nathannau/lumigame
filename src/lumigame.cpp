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

// Debounces the raw TCA9555 reading into `buttonState`: a bit is only
// committed once its raw value has stayed unchanged for BUTTON_DEBOUNCE_MS,
// timed independently per button so one bouncing button doesn't delay the
// others.
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

TimerEntry timerRegistry[MAX_TIMERS];

static TimerEntry *findTimer(const char *name, bool createIfMissing)
{
  for (uint8_t i = 0; i < MAX_TIMERS; i++)
  {
    if (timerRegistry[i].active && strcmp(timerRegistry[i].name, name) == 0)
    {
      return &timerRegistry[i];
    }
  }
  if (!createIfMissing)
    return nullptr;
  for (uint8_t i = 0; i < MAX_TIMERS; i++)
  {
    if (!timerRegistry[i].active)
    {
      timerRegistry[i].active = true;
      strncpy(timerRegistry[i].name, name, sizeof(timerRegistry[i].name) - 1);
      timerRegistry[i].name[sizeof(timerRegistry[i].name) - 1] = '\0';
      return &timerRegistry[i];
    }
  }
  return nullptr;
}

void lumigameTimerSet(const char *name, unsigned long ms)
{
  TimerEntry *t = findTimer(name, true);
  if (t)
    t->expireAt = millis() + ms;
}

bool lumigameTimerExpired(const char *name)
{
  TimerEntry *t = findTimer(name, false);
  if (!t)
    return true;
  return (long)(millis() - t->expireAt) >= 0;
}

unsigned long lumigameTimerRemaining(const char *name)
{
  TimerEntry *t = findTimer(name, false);
  if (!t)
    return 0;
  long remaining = (long)(t->expireAt - millis());
  return remaining > 0 ? (unsigned long)remaining : 0;
}

// Tiny 4-row x 3-column font, digits 0-9 then A-Z. One 4-bit value per
// column, written so the binary literal reads top-to-bottom for easy
// editing: bit 3 = row 0 (top) ... bit 0 = row 3 (bottom).
// Digits 1-6 are confirmed exact; 7/8/9/0 and all letters are placeholders
// to be corrected by hand.
#define FONT_GLYPH_WIDTH 3
#define FONT_CHAR_SPACING 1

static const uint8_t FONT_GLYPHS[36][FONT_GLYPH_WIDTH] = {
    /* 0 */ {0b1111, 0b1001, 0b1111},
    /* 1 */ {0b0101, 0b1111, 0b0001},
    /* 2 */ {0b1011, 0b1010, 0b1110},
    /* 3 */ {0b1001, 0b1011, 0b1111},
    /* 4 */ {0b0110, 0b1011, 0b0010},
    /* 5 */ {0b1110, 0b1010, 0b1011},
    /* 6 */ {0b1111, 0b1011, 0b1011},
    /* 7 */ {0b1000, 0b1010, 0b1111},
    /* 8 */ {0b1111, 0b1011, 0b1111},
    /* 9 */ {0b1101, 0b1101, 0b1111},
    /* A */ {0b0111, 0b1010, 0b0111},
    /* B */ {0b1111, 0b1011, 0b0111},
    /* C */ {0b1111, 0b1001, 0b1001},
    /* D */ {0b1111, 0b1001, 0b0110},
    /* E */ {0b1111, 0b1011, 0b1001},
    /* F */ {0b1111, 0b1010, 0b1000},
    /* G */ {0b1111, 0b1001, 0b1011},
    /* H */ {0b1111, 0b0010, 0b1111},
    /* I */ {0b1001, 0b1111, 0b1001},
    /* J */ {0b1001, 0b1111, 0b1000},
    /* K */ {0b1111, 0b0010, 0b1101},
    /* L */ {0b1111, 0b0001, 0b0001},
    /* M */ {0b1111, 0b0100, 0b1111},
    /* N */ {0b1111, 0b0110, 0b1111},
    /* O */ {0b0110, 0b1001, 0b0110},
    /* P */ {0b1111, 0b1010, 0b0100},
    /* Q */ {0b0110, 0b1001, 0b0111},
    /* R */ {0b1111, 0b1010, 0b0101},
    /* S */ {0b1101, 0b1011, 0b1011},
    /* T */ {0b1000, 0b1111, 0b1000},
    /* U */ {0b1111, 0b0001, 0b1111},
    /* V */ {0b1110, 0b0001, 0b1110},
    /* W */ {0b1110, 0b0111, 0b1110},
    /* X */ {0b1001, 0b0110, 0b1001},
    /* Y */ {0b1100, 0b0011, 0b1100},
    /* Z */ {0b1011, 0b1111, 0b1101},
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
// of FONT_GLYPH_WIDTH glyph columns plus FONT_CHAR_SPACING blank columns
// per character.
static bool lumigameFontPixelAt(const String &text, uint16_t col, uint8_t row)
{
  const uint16_t charAdvance = FONT_GLYPH_WIDTH + FONT_CHAR_SPACING;
  uint16_t charIndex = col / charAdvance;
  uint16_t colInChar = col % charAdvance;
  if (charIndex >= text.length() || colInChar >= FONT_GLYPH_WIDTH)
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

  const uint16_t charAdvance = FONT_GLYPH_WIDTH + FONT_CHAR_SPACING;
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
    }
  }

  setAllLeds(CRGB::Black);
  for (uint8_t dc = 0; dc < NUM_COLS; dc++)
  {
    int32_t textCol = (int32_t)dc + scrollOffset - NUM_COLS;
    if (textCol < 0 || textCol >= (int32_t)textWidth)
      continue;
    for (uint8_t row = 0; row < NUM_ROWS; row++)
    {
      if (lumigameFontPixelAt(scrollActiveText, (uint16_t)textCol, row))
      {
        setLedColor(row * NUM_COLS + dc, color);
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
