#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <math.h>

// ================= Pin configuration =================
#define PIN_LDR        0
#define PIN_RGBW       4
#define PIN_BACKLIGHT  5

#define PIN_CP_A       6
#define PIN_CP_B       7
#define PIN_CP_C       10

#define BTN_RGBW       20
#define BTN_BACKLIGHT  21

#define NUM_LED        30
#define LONG_SIDE      20
#define SHORT_SIDE     10

// ================= RGBW strip =================
Adafruit_NeoPixel strip(NUM_LED, PIN_RGBW, NEO_GRBW + NEO_KHZ800);

// ================= PWM =================
#define PWM_CH 0

// ================= Persistent storage =================
Preferences prefs;

// ================= RGB modes =================
enum RGBMode {
  AMBER,
  RAINBOW,
  L_COMET,
  SPLIT_CORNER,
  CORNER,
  SOFT_DRIFT,
  PASTEL_FLOW,
  CYAN,
  PURPLE,
  OCEAN_FLOW,
  SUNSET_FLOW,
  AURORA,
  GOLD_WAVE,
  MINT_LAVENDER,
  DUAL_EDGE_FLOW,
  SOFT_ROTATE,
  BREATH_WHITE,
  BREATH_WARM,
  PALETTE_SHOW,
  SINGLE_PIXEL_RING,
  NIGHT,
  OFF,
  RGB_COUNT
};

// ================= Charlieplex modes =================
enum CPMode {
  CP_RUN,
  CP_BOUNCE,
  CP_PAIR,
  CP_SKIP,
  CP_MODE,
  CP_COUNT
};

// ================= Backlight modes =================
enum BLMode {
  BL_OFF,
  BL_MED,
  BL_HIGH,
  BL_COUNT
};

// ================= Runtime state =================
RGBMode rgbMode = RAINBOW;
CPMode cpMode = CP_RUN;
BLMode blMode = BL_MED;

uint8_t hue = 0;
uint16_t stepAnim = 0;

uint8_t cpIndex = 0;
int8_t cpDir = 1;

uint8_t cpSpeedIdx = 1;
uint16_t cpSpeeds[] = {180, 120, 75};

uint8_t brightnessIdx = 3;
uint8_t brightnessLevels[] = {35, 65, 95, 130, 170};

bool ldrEnabled = true;

// ================= Timers =================
uint32_t tRGB = 0;
uint32_t tCP = 0;

// ================= Button multi-click =================
struct MultiClickButton {
  uint8_t pin;
  bool lastReading;
  bool stable;
  uint32_t lastDebounce;
  uint8_t clicks;
  uint32_t lastClickMs;

  MultiClickButton(uint8_t p) {
    pin = p;
    lastReading = HIGH;
    stable = HIGH;
    lastDebounce = 0;
    clicks = 0;
    lastClickMs = 0;
  }
};

MultiClickButton btnRGB(BTN_RGBW);
MultiClickButton btnBL(BTN_BACKLIGHT);

#define DEBOUNCE 30
#define CLICK_TIME 420

// ================= Auto demo =================
bool autoDemoMode = false;
uint32_t lastDemoStepMs = 0;
const uint32_t demoIntervalMs = 6500;

// Saved state before demo starts
RGBMode savedRgbMode = RAINBOW;
CPMode savedCpMode = CP_RUN;
BLMode savedBlMode = BL_MED;
uint8_t savedCpSpeedIdx = 1;
uint8_t savedBrightnessIdx = 3;
bool savedLdrEnabled = true;

// Demo progress: two full loops through RGB modes
uint16_t demoStepCount = 0;
const uint16_t demoTotalSteps = RGB_COUNT * 2;

// Combined button hold detection
bool rgbPressActive = false;
bool blPressActive = false;

uint32_t rgbPressStartMs = 0;
uint32_t blPressStartMs = 0;

uint32_t bothHoldStartMs = 0;
bool bothHoldTriggered = false;

const uint32_t dualPressSyncWindowMs = 450;
const uint32_t dualPressHoldMs = 1200;

// ================= Save/load helpers =================
void saveState() {
  prefs.putUChar("rgbMode", (uint8_t)rgbMode);
  prefs.putUChar("cpMode", (uint8_t)cpMode);
  prefs.putUChar("blMode", (uint8_t)blMode);
  prefs.putUChar("cpSpeed", cpSpeedIdx);
  prefs.putUChar("bright", brightnessIdx);
  prefs.putBool("ldr", ldrEnabled);
}

void loadState() {
  uint8_t vRgb = prefs.getUChar("rgbMode", (uint8_t)RAINBOW);
  uint8_t vCp = prefs.getUChar("cpMode", (uint8_t)CP_RUN);
  uint8_t vBl = prefs.getUChar("blMode", (uint8_t)BL_MED);
  uint8_t vSpeed = prefs.getUChar("cpSpeed", 1);
  uint8_t vBright = prefs.getUChar("bright", 3);
  bool vLdr = prefs.getBool("ldr", true);

  if (vRgb < RGB_COUNT) rgbMode = (RGBMode)vRgb;
  if (vCp < CP_COUNT) cpMode = (CPMode)vCp;
  if (vBl < BL_COUNT) blMode = (BLMode)vBl;
  if (vSpeed < 3) cpSpeedIdx = vSpeed;
  if (vBright < 5) brightnessIdx = vBright;
  ldrEnabled = vLdr;
}

// ================= Color helpers =================
uint32_t cRGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  return strip.Color(r, g, b, w);
}

void clearAll() {
  for (int i = 0; i < NUM_LED; i++) {
    strip.setPixelColor(i, 0);
  }
}

void fillSolid(uint32_t c) {
  for (int i = 0; i < NUM_LED; i++) {
    strip.setPixelColor(i, c);
  }
}

uint32_t wheel(byte pos) {
  pos = 255 - pos;
  if (pos < 85) return cRGBW((255 - pos * 3) / 2, 0, (pos * 3) / 2, 0);
  if (pos < 170) {
    pos -= 85;
    return cRGBW(0, (pos * 3) / 2, (255 - pos * 3) / 2, 0);
  }
  pos -= 170;
  return cRGBW((pos * 3) / 2, (255 - pos * 3) / 2, 0, 0);
}

uint32_t richWheel(byte pos) {
  pos = 255 - pos;
  if (pos < 85) return cRGBW(255 - pos * 3, 0, pos * 3, 0);
  if (pos < 170) {
    pos -= 85;
    return cRGBW(0, pos * 3, 255 - pos * 3, 0);
  }
  pos -= 170;
  return cRGBW(pos * 3, 255 - pos * 3, 0, 0);
}

uint32_t pastelWheel(byte pos) {
  switch ((pos / 42) % 6) {
    case 0: return cRGBW(40, 8, 12, 80);
    case 1: return cRGBW(12, 35, 55, 35);
    case 2: return cRGBW(50, 18, 0, 35);
    case 3: return cRGBW(30, 0, 45, 20);
    case 4: return cRGBW(0, 25, 35, 60);
    default:return cRGBW(10, 5, 0, 100);
  }
}

uint32_t cometPalette(uint8_t phase) {
  switch ((phase / 24) % 10) {
    case 0: return cRGBW(0, 0, 0, 170);
    case 1: return cRGBW(120, 30, 0, 0);
    case 2: return cRGBW(0, 120, 160, 0);
    case 3: return cRGBW(90, 0, 130, 0);
    case 4: return cRGBW(0, 0, 90, 18);
    case 5: return cRGBW(140, 0, 30, 0);
    case 6: return cRGBW(0, 90, 40, 30);
    case 7: return cRGBW(50, 10, 80, 35);
    case 8: return cRGBW(0, 35, 110, 0);
    default:return cRGBW(100, 50, 0, 35);
  }
}

void scaleColor(uint32_t c, float k, uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &w) {
  uint8_t cr = (uint8_t)(c >> 24);
  uint8_t cg = (uint8_t)(c >> 16);
  uint8_t cb = (uint8_t)(c >> 8);
  uint8_t cw = (uint8_t)(c);

  r = cr * k;
  g = cg * k;
  b = cb * k;
  w = cw * k;
}

// ================= Brightness helpers =================
uint16_t getLdrRaw() {
  return analogRead(PIN_LDR);
}

uint8_t getRGBBrightness() {
  if (ldrEnabled) {
    int raw = getLdrRaw();
    return constrain(map(raw, 0, 4095, 18, 140), 10, 150);
  }
  return brightnessLevels[brightnessIdx];
}

uint8_t getBacklight() {
  if (ldrEnabled) {
    int raw = getLdrRaw();
    return constrain(map(raw, 4095, 0, 20, 255), 0, 255);
  }

  switch (blMode) {
    case BL_OFF:  return 0;
    case BL_MED:  return 120;
    case BL_HIGH: return 255;
  }

  return 0;
}

// ================= RGB mode renderers =================
void renderLComet() {
  clearAll();
  int pos = stepAnim % (NUM_LED + 10);
  uint32_t base = cometPalette(hue);

  for (int i = 0; i < LONG_SIDE; i++) {
    int d = pos - i;
    if (d >= 0 && d < 10) {
      float k = 1.0f - (d * 0.10f);
      uint8_t r, g, b, w;
      scaleColor(base, k, r, g, b, w);
      strip.setPixelColor(i, cRGBW(r, g, b, w));
    }
  }

  for (int i = 0; i < SHORT_SIDE; i++) {
    int idx = LONG_SIDE + i;
    int d = pos - idx;
    if (d >= 0 && d < 10) {
      float k = 1.0f - (d * 0.10f);
      uint8_t r, g, b, w;
      scaleColor(base, k, r, g, b, w);
      strip.setPixelColor(idx, cRGBW(r, g, b, w));
    }
  }
}

void renderSplitCorner() {
  clearAll();
  int pos = stepAnim % (LONG_SIDE + 10);
  uint32_t base = cometPalette(hue / 2);

  for (int i = 0; i < LONG_SIDE; i++) {
    int ledIndex = 19 - i;
    int d = pos - i;
    if (d >= 0 && d < 10) {
      float k = 1.0f - (d * 0.10f);
      uint8_t r, g, b, w;
      scaleColor(base, k, r, g, b, w);
      strip.setPixelColor(ledIndex, cRGBW(r, g, b, w));
    }
  }

  for (int i = 0; i < SHORT_SIDE; i++) {
    int ledIndex = 20 + i;
    int d = pos - i;
    if (d >= 0 && d < 10) {
      float k = 1.0f - (d * 0.10f);
      uint8_t r, g, b, w;
      scaleColor(base, k, r, g, b, w);
      strip.setPixelColor(ledIndex, cRGBW(r, g, b, w));
    }
  }

  uint32_t center = cometPalette(hue + 80);
  uint8_t r, g, b, w;
  scaleColor(center, 0.45f, r, g, b, w);
  strip.setPixelColor(19, cRGBW(r, g, b, w));
  strip.setPixelColor(20, cRGBW(r, g, b, w));
}

void renderCorner() {
  clearAll();

  uint32_t base = cometPalette(hue + 40);
  float s = (sin(stepAnim * 0.03f) + 1.0f) * 0.5f;
  float boost = 0.35f + s * 0.55f;

  for (int i = 0; i < LONG_SIDE; i++) {
    int dist = abs(19 - i);
    float k = max(0.0f, boost - dist * 0.05f);
    uint8_t r, g, b, w;
    scaleColor(base, k, r, g, b, w);
    strip.setPixelColor(i, cRGBW(r, g, b, w));
  }

  uint32_t base2 = cometPalette(hue + 120);
  for (int i = 0; i < SHORT_SIDE; i++) {
    float k = max(0.0f, boost - i * 0.08f);
    uint8_t r, g, b, w;
    scaleColor(base2, k, r, g, b, w);
    strip.setPixelColor(LONG_SIDE + i, cRGBW(r, g, b, w));
  }
}

void renderSoftDrift() {
  for (int i = 0; i < NUM_LED; i++) {
    uint8_t localHue = hue + i * 3;
    uint32_t c = cometPalette(localHue);
    uint8_t r, g, b, w;
    scaleColor(c, 0.45f, r, g, b, w);
    strip.setPixelColor(i, cRGBW(r, g, b, w));
  }
}

void renderPastelFlow() {
  for (int i = 0; i < NUM_LED; i++) {
    strip.setPixelColor(i, pastelWheel((hue / 2) + i * 5));
  }
}

void renderOceanFlow() {
  for (int i = 0; i < NUM_LED; i++) {
    uint8_t ph = hue + i * 4;
    switch ((ph / 36) % 5) {
      case 0: strip.setPixelColor(i, cRGBW(0, 35, 110, 20)); break;
      case 1: strip.setPixelColor(i, cRGBW(0, 80, 120, 0)); break;
      case 2: strip.setPixelColor(i, cRGBW(0, 0, 90, 20)); break;
      case 3: strip.setPixelColor(i, cRGBW(0, 25, 65, 65)); break;
      default:strip.setPixelColor(i, cRGBW(0, 55, 75, 35)); break;
    }
  }
}

void renderSunsetFlow() {
  for (int i = 0; i < NUM_LED; i++) {
    uint8_t ph = (hue / 2) + i * 4;
    switch ((ph / 40) % 6) {
      case 0: strip.setPixelColor(i, cRGBW(120, 25, 0, 0)); break;
      case 1: strip.setPixelColor(i, cRGBW(90, 0, 45, 0)); break;
      case 2: strip.setPixelColor(i, cRGBW(70, 10, 0, 55)); break;
      case 3: strip.setPixelColor(i, cRGBW(0, 0, 65, 10)); break;
      case 4: strip.setPixelColor(i, cRGBW(100, 35, 0, 25)); break;
      default:strip.setPixelColor(i, cRGBW(55, 0, 25, 20)); break;
    }
  }
}

void renderAurora() {
  for (int i = 0; i < NUM_LED; i++) {
    float s1 = (sin((stepAnim + i * 6) * 0.03f) + 1.0f) * 0.5f;
    float s2 = (sin((stepAnim + i * 4) * 0.02f + 2.0f) + 1.0f) * 0.5f;
    uint8_t g = uint8_t(20 + s1 * 90);
    uint8_t b = uint8_t(35 + s2 * 95);
    uint8_t w = uint8_t(6 + s1 * 22);
    strip.setPixelColor(i, cRGBW(0, g, b, w));
  }
}

void renderGoldWave() {
  for (int i = 0; i < NUM_LED; i++) {
    float s = (sin((stepAnim + i * 7) * 0.035f) + 1.0f) * 0.5f;
    uint8_t r = uint8_t(40 + s * 55);
    uint8_t g = uint8_t(15 + s * 25);
    uint8_t w = uint8_t(50 + s * 80);
    uint8_t b = uint8_t(5 + s * 15);
    strip.setPixelColor(i, cRGBW(r, g, b, w));
  }
}

void renderMintLavender() {
  for (int i = 0; i < NUM_LED; i++) {
    uint8_t ph = (hue / 2) + i * 6;
    if ((ph / 64) % 2 == 0) {
      strip.setPixelColor(i, cRGBW(0, 70, 55, 40));
    } else {
      strip.setPixelColor(i, cRGBW(55, 0, 75, 25));
    }
  }
}

void renderDualEdgeFlow() {
  clearAll();
  int pos1 = stepAnim % LONG_SIDE;
  int pos2 = stepAnim % SHORT_SIDE;
  uint32_t c1 = cometPalette(hue);
  uint32_t c2 = cometPalette(hue + 100);

  for (int t = 0; t < 6; t++) {
    int i1 = pos1 - t;
    if (i1 >= 0) {
      uint8_t r, g, b, w;
      scaleColor(c1, 1.0f - t * 0.14f, r, g, b, w);
      strip.setPixelColor(i1, cRGBW(r, g, b, w));
    }

    int i2 = LONG_SIDE + pos2 - t;
    if (i2 >= LONG_SIDE) {
      uint8_t r, g, b, w;
      scaleColor(c2, 1.0f - t * 0.14f, r, g, b, w);
      strip.setPixelColor(i2, cRGBW(r, g, b, w));
    }
  }
}

void renderSoftRotate() {
  for (int i = 0; i < NUM_LED; i++) {
    int ringPos = (i + stepAnim / 2) % NUM_LED;
    uint32_t c = cometPalette(ringPos * 8 + hue);
    uint8_t r, g, b, w;
    scaleColor(c, 0.55f, r, g, b, w);
    strip.setPixelColor(i, cRGBW(r, g, b, w));
  }
}

void renderBreathWhite() {
  float s = (sin(stepAnim * 0.03f) + 1.0f) * 0.5f;
  uint8_t w = uint8_t(20 + s * 180);
  fillSolid(cRGBW(0, 0, 20, w));
}

void renderBreathWarm() {
  float s = (sin(stepAnim * 0.03f) + 1.0f) * 0.5f;
  uint8_t w = uint8_t(15 + s * 120);
  uint8_t r = uint8_t(6 + s * 18);
  fillSolid(cRGBW(r, 4, 6, w));
}

void renderPaletteShow() {
  static const uint32_t palette[] = {
    cRGBW(255, 0, 0, 0),
    cRGBW(255, 120, 0, 0),
    cRGBW(255, 220, 0, 0),
    cRGBW(0, 180, 40, 0),
    cRGBW(0, 180, 255, 0),
    cRGBW(0, 0, 200, 20),
    cRGBW(120, 0, 220, 0),
    cRGBW(255, 0, 120, 0),
    cRGBW(0, 0, 0, 255),
    cRGBW(255, 180, 0, 60)
  };

  int count = sizeof(palette) / sizeof(palette[0]);
  int block = NUM_LED / count;
  if (block < 1) block = 1;

  int shift = (stepAnim / 8) % NUM_LED;

  for (int i = 0; i < NUM_LED; i++) {
    int idx = (i + shift) % NUM_LED;
    int p = (i / block) % count;
    strip.setPixelColor(idx, palette[p]);
  }
}

void renderSinglePixelRing() {
  clearAll();

  int pos = stepAnim % NUM_LED;
  uint32_t c = richWheel((hue * 2) & 255);

  strip.setPixelColor(pos, c);

  int prev = (pos - 1 + NUM_LED) % NUM_LED;
  uint8_t r, g, b, w;
  scaleColor(c, 0.20f, r, g, b, w);
  strip.setPixelColor(prev, cRGBW(r, g, b, w));
}

void renderRGB() {
  strip.setBrightness(getRGBBrightness());

  switch (rgbMode) {
    case AMBER:            fillSolid(cRGBW(130, 35, 0, 0)); break;
    case RAINBOW:
      for (int i = 0; i < NUM_LED; i++) strip.setPixelColor(i, wheel((hue + i * 4) & 255));
      break;
    case L_COMET:           renderLComet(); break;
    case SPLIT_CORNER:      renderSplitCorner(); break;
    case CORNER:            renderCorner(); break;
    case SOFT_DRIFT:        renderSoftDrift(); break;
    case PASTEL_FLOW:       renderPastelFlow(); break;
    case CYAN:              fillSolid(cRGBW(0, 110, 150, 0)); break;
    case PURPLE:            fillSolid(cRGBW(90, 0, 120, 0)); break;
    case OCEAN_FLOW:        renderOceanFlow(); break;
    case SUNSET_FLOW:       renderSunsetFlow(); break;
    case AURORA:            renderAurora(); break;
    case GOLD_WAVE:         renderGoldWave(); break;
    case MINT_LAVENDER:     renderMintLavender(); break;
    case DUAL_EDGE_FLOW:    renderDualEdgeFlow(); break;
    case SOFT_ROTATE:       renderSoftRotate(); break;
    case BREATH_WHITE:      renderBreathWhite(); break;
    case BREATH_WARM:       renderBreathWarm(); break;
    case PALETTE_SHOW:      renderPaletteShow(); break;
    case SINGLE_PIXEL_RING: renderSinglePixelRing(); break;
    case NIGHT:
      strip.setBrightness(16);
      fillSolid(cRGBW(3, 1, 0, 6));
      break;
    case OFF:               clearAll(); break;
  }

  strip.show();
}

// ================= Charlieplex =================
void cpDrive(uint8_t h, uint8_t l, uint8_t z) {
  pinMode(h, OUTPUT);
  pinMode(l, OUTPUT);
  pinMode(z, INPUT);
  digitalWrite(h, HIGH);
  digitalWrite(l, LOW);
}

void cpShow(uint8_t i) {
  switch (i % 6) {
    case 0: cpDrive(PIN_CP_A, PIN_CP_B, PIN_CP_C); break;
    case 1: cpDrive(PIN_CP_B, PIN_CP_A, PIN_CP_C); break;
    case 2: cpDrive(PIN_CP_B, PIN_CP_C, PIN_CP_A); break;
    case 3: cpDrive(PIN_CP_C, PIN_CP_B, PIN_CP_A); break;
    case 4: cpDrive(PIN_CP_A, PIN_CP_C, PIN_CP_B); break;
    case 5: cpDrive(PIN_CP_C, PIN_CP_A, PIN_CP_B); break;
  }
}

void updateCP() {
  switch (cpMode) {
    case CP_RUN:    cpShow(cpIndex); break;
    case CP_BOUNCE: cpShow(cpIndex); break;
    case CP_PAIR:   cpShow((cpIndex / 2) % 6); break;
    case CP_SKIP:   cpShow((cpIndex * 2) % 6); break;
    case CP_MODE:   cpShow(rgbMode % 6); break;
  }
}

// ================= Button logic =================
struct ClickHandler {
  static void rgbClicks(uint8_t c) {
    if (c == 1) {
      rgbMode = (RGBMode)((rgbMode + 1) % RGB_COUNT);
      saveState();
    } else if (c == 2) {
      cpMode = (CPMode)((cpMode + 1) % CP_COUNT);
      saveState();
    } else if (c == 3) {
      cpSpeedIdx = (cpSpeedIdx + 1) % 3;
      saveState();
    }
  }

  static void blClicks(uint8_t c) {
    if (c == 1) {
      ldrEnabled = false;
      blMode = (BLMode)((blMode + 1) % BL_COUNT);
      saveState();
    } else if (c == 2) {
      brightnessIdx = (brightnessIdx + 1) % 5;
      saveState();
    } else if (c == 3) {
      ldrEnabled = !ldrEnabled;
      saveState();
    }
  }
};

void handleClicks(MultiClickButton &b, void (*fn)(uint8_t)) {
  bool r = digitalRead(b.pin);

  if (r != b.lastReading) {
    b.lastDebounce = millis();
    b.lastReading = r;
  }

  if (millis() - b.lastDebounce > DEBOUNCE) {
    if (r != b.stable) {
      b.stable = r;

      // Count clicks on button release
      if (r == HIGH) {
        b.clicks++;
        b.lastClickMs = millis();
      }
    }
  }

  // Execute action after no new clicks have arrived for the timeout window
  if (b.clicks > 0 && (millis() - b.lastClickMs > CLICK_TIME)) {
    fn(b.clicks);
    b.clicks = 0;
  }
}

// ================= Auto demo =================
void startAutoDemo() {
  savedRgbMode = rgbMode;
  savedCpMode = cpMode;
  savedBlMode = blMode;
  savedCpSpeedIdx = cpSpeedIdx;
  savedBrightnessIdx = brightnessIdx;
  savedLdrEnabled = ldrEnabled;

  autoDemoMode = true;
  demoStepCount = 0;
  lastDemoStepMs = millis();
}

void stopAutoDemoAndRestore() {
  autoDemoMode = false;

  rgbMode = savedRgbMode;
  cpMode = savedCpMode;
  blMode = savedBlMode;
  cpSpeedIdx = savedCpSpeedIdx;
  brightnessIdx = savedBrightnessIdx;
  ldrEnabled = savedLdrEnabled;
}

void updateAutoDemoToggle() {
  bool rgbPressed = (digitalRead(BTN_RGBW) == LOW);
  bool blPressed  = (digitalRead(BTN_BACKLIGHT) == LOW);

  if (rgbPressed && !rgbPressActive) {
    rgbPressActive = true;
    rgbPressStartMs = millis();
  }
  if (!rgbPressed) {
    rgbPressActive = false;
  }

  if (blPressed && !blPressActive) {
    blPressActive = true;
    blPressStartMs = millis();
  }
  if (!blPressed) {
    blPressActive = false;
  }

  if (rgbPressed && blPressed) {
    uint32_t pressDelta = (rgbPressStartMs > blPressStartMs)
                        ? (rgbPressStartMs - blPressStartMs)
                        : (blPressStartMs - rgbPressStartMs);

    if (pressDelta <= dualPressSyncWindowMs) {
      if (bothHoldStartMs == 0) {
        bothHoldStartMs = millis();
      }

      if (!bothHoldTriggered && (millis() - bothHoldStartMs >= dualPressHoldMs)) {
        if (autoDemoMode) {
          stopAutoDemoAndRestore();
        } else {
          startAutoDemo();
        }
        bothHoldTriggered = true;
      }
    } else {
      bothHoldStartMs = 0;
      bothHoldTriggered = false;
    }
  } else {
    bothHoldStartMs = 0;
    bothHoldTriggered = false;
  }
}

void updateAutoDemo() {
  if (!autoDemoMode) return;

  if (millis() - lastDemoStepMs >= demoIntervalMs) {
    lastDemoStepMs = millis();

    rgbMode = (RGBMode)((rgbMode + 1) % RGB_COUNT);
    cpMode = (CPMode)((cpMode + 1) % CP_COUNT);
    cpSpeedIdx = (cpSpeedIdx + 1) % 3;
    blMode = (BLMode)((blMode + 1) % BL_COUNT);
    brightnessIdx = (brightnessIdx + 1) % 5;

    demoStepCount++;

    if (demoStepCount >= demoTotalSteps) {
      stopAutoDemoAndRestore();
    }
  }
}

// ================= Setup =================
void setup() {
  pinMode(BTN_RGBW, INPUT_PULLUP);
  pinMode(BTN_BACKLIGHT, INPUT_PULLUP);

  analogReadResolution(12);

  strip.begin();
  strip.show();

  ledcSetup(PWM_CH, 5000, 8);
  ledcAttachPin(PIN_BACKLIGHT, PWM_CH);

  prefs.begin("frame", false);
  loadState();
}

// ================= Main loop =================
void loop() {
  updateAutoDemoToggle();

  if (!autoDemoMode) {
    handleClicks(btnRGB, ClickHandler::rgbClicks);
    handleClicks(btnBL, ClickHandler::blClicks);
  } else {
    updateAutoDemo();
  }

  if (millis() - tRGB > 35) {
    tRGB = millis();
    hue++;
    stepAnim++;
    renderRGB();
  }

  ledcWrite(PWM_CH, getBacklight());

  if (millis() - tCP > cpSpeeds[cpSpeedIdx]) {
    tCP = millis();

    if (cpMode == CP_BOUNCE) {
      cpIndex += cpDir;
      if (cpIndex >= 5) { cpIndex = 5; cpDir = -1; }
      if (cpIndex == 0)  { cpDir = 1; }
    } else {
      cpIndex++;
    }
  }

  updateCP();
}