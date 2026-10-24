#include "hal.h"

#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

TFT_eSPI tft = TFT_eSPI();
Settings settings;
Touch touch;

// ======================= settings =======================
static Preferences prefs;

void settingsLoad() {
  prefs.begin("cyd", true);
  settings.brightness = prefs.getUChar("bright", settings.brightness);
  settings.sound = prefs.getBool("sound", settings.sound);
  settings.autoDim = prefs.getBool("autodim", settings.autoDim);
  settings.serverUrl = prefs.getString("server", settings.serverUrl);
  settings.calibrated = prefs.getBool("cal", false);
  if (settings.calibrated) {
    settings.calXMin = prefs.getShort("calxmin", settings.calXMin);
    settings.calXMax = prefs.getShort("calxmax", settings.calXMax);
    settings.calYMin = prefs.getShort("calymin", settings.calYMin);
    settings.calYMax = prefs.getShort("calymax", settings.calYMax);
  }
  prefs.end();
  if (settings.brightness < 20) settings.brightness = 20;
}

void settingsSave() {
  prefs.begin("cyd", false);
  prefs.putUChar("bright", settings.brightness);
  prefs.putBool("sound", settings.sound);
  prefs.putBool("autodim", settings.autoDim);
  prefs.putString("server", settings.serverUrl);
  prefs.putBool("cal", settings.calibrated);
  if (settings.calibrated) {
    prefs.putShort("calxmin", settings.calXMin);
    prefs.putShort("calxmax", settings.calXMax);
    prefs.putShort("calymin", settings.calYMin);
    prefs.putShort("calymax", settings.calYMax);
  }
  prefs.end();
}

uint32_t prefGetU32(const char *key, uint32_t def) {
  prefs.begin("cyd", true);
  uint32_t v = prefs.getUInt(key, def);
  prefs.end();
  return v;
}

void prefPutU32(const char *key, uint32_t value) {
  prefs.begin("cyd", false);
  prefs.putUInt(key, value);
  prefs.end();
}

// ======================= touch =======================
// The touch controller has its own SPI bus (VSPI); the display uses HSPI (-DUSE_HSPI_PORT).
static SPIClass touchSPI(VSPI);
static XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

void Touch::applyCalibration() {
  xMin_ = settings.calXMin; xMax_ = settings.calXMax;
  yMin_ = settings.calYMin; yMax_ = settings.calYMax;
}

void Touch::begin() {
  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);
  applyCalibration();
}

bool Touch::rawPoint(int &rx, int &ry) {
  if (!(ts.tirqTouched() && ts.touched())) return false;
  TS_Point p = ts.getPoint();
  rx = p.x;
  ry = p.y;
  return true;
}

void Touch::poll() {
  pressedEdge_ = false;
  releasedEdge_ = false;
  uint32_t now = millis();
  int rx, ry;
  if (rawPoint(rx, ry)) {
    int px = map(rx, xMin_, xMax_, 0, SCREEN_W - 1);
    int py = map(ry, yMin_, yMax_, 0, SCREEN_H - 1);
    x_ = constrain(px, 0, SCREEN_W - 1);
    y_ = constrain(py, 0, SCREEN_H - 1);
    lastSeen_ = now;
    if (!down_) {
      // The first sample of a touch is often off; wait for a second one before reporting it.
      if (++stable_ >= 2) {
        down_ = true;
        pressedEdge_ = !consumed_;
      }
    }
  } else {
    stable_ = 0;
    if (down_ && now - lastSeen_ > 40) {
      down_ = false;
      releasedEdge_ = true;
      consumed_ = false;
    }
  }
}

// Interactive calibration: tap four crosshairs near the corners.
void touchCalibrate() {
  const int inset = 20;
  const int targets[4][2] = {{inset, inset}, {SCREEN_W - inset, inset}, {inset, SCREEN_H - inset}, {SCREEN_W - inset, SCREEN_H - inset}};
  long sumX[4] = {0}, sumY[4] = {0};

  for (int i = 0; i < 4; i++) {
    tft.fillScreen(C_BG);
    tft.setTextColor(C_TEXT, C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Touch calibration", SCREEN_W / 2, 90, 4);
    tft.drawString("Tap the centre of each cross", SCREEN_W / 2, 125, 2);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d / 4", i + 1);
    tft.drawString(buf, SCREEN_W / 2, 150, 2);
    int tx = targets[i][0], ty = targets[i][1];
    tft.drawLine(tx - 10, ty, tx + 10, ty, C_ACCENT);
    tft.drawLine(tx, ty - 10, tx, ty + 10, C_ACCENT);
    tft.drawCircle(tx, ty, 6, C_ACCENT);
    tft.setTextDatum(TL_DATUM);

    // wait for the finger to be lifted from the previous tap
    int rx, ry;
    while (touch.rawPoint(rx, ry)) delay(10);
    delay(150);
    // wait for a touch, then average a short burst of samples
    while (!touch.rawPoint(rx, ry)) delay(5);
    long sx = 0, sy = 0;
    int n = 0;
    uint32_t t0 = millis();
    while (millis() - t0 < 120) {
      if (touch.rawPoint(rx, ry)) { sx += rx; sy += ry; n++; }
      delay(4);
    }
    if (n == 0) { i--; continue; }
    sumX[i] = sx / n;
    sumY[i] = sy / n;
    sound::click();
  }

  // raw values at the left/right and top/bottom target lines
  float rawL = (sumX[0] + sumX[2]) / 2.0f, rawR = (sumX[1] + sumX[3]) / 2.0f;
  float rawT = (sumY[0] + sumY[1]) / 2.0f, rawB = (sumY[2] + sumY[3]) / 2.0f;
  float slopeX = (rawR - rawL) / (float)(SCREEN_W - 2 * inset);
  float slopeY = (rawB - rawT) / (float)(SCREEN_H - 2 * inset);
  settings.calXMin = (int16_t)(rawL - inset * slopeX);
  settings.calXMax = (int16_t)(rawL + (SCREEN_W - 1 - inset) * slopeX);
  settings.calYMin = (int16_t)(rawT - inset * slopeY);
  settings.calYMax = (int16_t)(rawT + (SCREEN_H - 1 - inset) * slopeY);
  settings.calibrated = true;
  settingsSave();
  touch.applyCalibration();
  Serial.printf("Calibration: x %d..%d  y %d..%d\n", settings.calXMin, settings.calXMax, settings.calYMin, settings.calYMax);
}

// ======================= backlight =======================
namespace backlight {
static const int CH = 0;
static uint8_t current = 0;
static float ldrAvg = -1;
static uint32_t lastSample = 0;

void begin() {
  ledcSetup(CH, 5000, 8);
  ledcAttachPin(PIN_BACKLIGHT, CH);
  pinMode(PIN_LDR, INPUT);
  set(settings.brightness);
}

void set(uint8_t level) {
  current = level;
  ledcWrite(CH, level);
}

void update() {
  if (!settings.autoDim) {
    if (current != settings.brightness) set(settings.brightness);
    return;
  }
  uint32_t now = millis();
  if (now - lastSample < 400) return;
  lastSample = now;
  float raw = analogRead(PIN_LDR) / 4095.0f;
  float light = LDR_DARK_IS_HIGH ? (1.0f - raw) : raw;   // 0 = dark, 1 = bright
  ldrAvg = (ldrAvg < 0) ? light : (ldrAvg * 0.8f + light * 0.2f);
  float factor = constrain(ldrAvg * 1.6f, 0.12f, 1.0f);
  int level = 20 + (int)((settings.brightness - 20) * factor);
  set((uint8_t)constrain(level, 20, 255));
}
}  // namespace backlight

// ======================= sound =======================
namespace sound {
struct Note { uint16_t freq; uint16_t ms; };   // freq 0 = rest
static const int CH = 1;
static const Note *seq = nullptr;
static uint8_t seqLen = 0, seqPos = 0;
static uint32_t noteEnd = 0;
static bool playing = false;

static const Note N_CLICK[] = {{1800, 12}};
static const Note N_GOOD[] = {{1200, 40}, {1700, 60}};
static const Note N_BAD[] = {{220, 120}};
static const Note N_EAT[] = {{900, 30}, {1300, 30}};
static const Note N_WIN[] = {{660, 90}, {880, 90}, {1100, 90}, {1320, 200}};
static const Note N_LOSE[] = {{440, 150}, {370, 150}, {294, 150}, {220, 350}};

void begin() {
  ledcSetup(CH, 2000, 8);
  ledcAttachPin(PIN_SPEAKER, CH);
  ledcWriteTone(CH, 0);
}

static void startNote() {
  const Note &n = seq[seqPos];
  if (n.freq) ledcWriteTone(CH, n.freq); else ledcWriteTone(CH, 0);
  noteEnd = millis() + n.ms;
}

static void play(const Note *s, uint8_t len) {
  if (!settings.sound) return;
  seq = s; seqLen = len; seqPos = 0; playing = true;
  startNote();
}

void update() {
  if (!playing || millis() < noteEnd) return;
  if (++seqPos >= seqLen) {
    playing = false;
    ledcWriteTone(CH, 0);
    ledcWrite(CH, 0);
    return;
  }
  startNote();
}

void click() { play(N_CLICK, 1); }
void good() { play(N_GOOD, 2); }
void bad() { play(N_BAD, 1); }
void eat() { play(N_EAT, 2); }
void win() { play(N_WIN, 4); }
void lose() { play(N_LOSE, 4); }
}  // namespace sound

// ======================= LED =======================
namespace led {
static uint32_t offAt = 0;
static bool on = false;

static void write(bool r, bool g, bool b) {
  digitalWrite(PIN_LED_R, r ? LOW : HIGH);
  digitalWrite(PIN_LED_G, g ? LOW : HIGH);
  digitalWrite(PIN_LED_B, b ? LOW : HIGH);
}

void begin() {
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  write(false, false, false);
}

void flash(bool r, bool g, bool b, uint16_t ms) {
  write(r, g, b);
  on = true;
  offAt = millis() + ms;
}

void update() {
  if (on && millis() >= offAt) {
    write(false, false, false);
    on = false;
  }
}
}  // namespace led
