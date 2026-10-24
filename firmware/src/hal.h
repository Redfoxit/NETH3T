#pragma once
// Hardware layer for the CYD: display, touch, backlight (+auto-dim), speaker, RGB LED, saved settings.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <esp_system.h>   // esp_random()
#include "config.h"

extern TFT_eSPI tft;

// ---------------- settings (persisted in NVS) ----------------
struct Settings {
  uint8_t brightness = 220;   // 20..255
  bool sound = true;
  bool autoDim = false;
  String serverUrl = DEFAULT_WORD_SERVER;
  bool calibrated = false;
  int16_t calXMin = TOUCH_RAW_X_MIN, calXMax = TOUCH_RAW_X_MAX;
  int16_t calYMin = TOUCH_RAW_Y_MIN, calYMax = TOUCH_RAW_Y_MAX;
};
extern Settings settings;
void settingsLoad();
void settingsSave();
uint32_t prefGetU32(const char *key, uint32_t def = 0);
void prefPutU32(const char *key, uint32_t value);

// ---------------- touch ----------------
class Touch {
 public:
  void begin();
  void poll();                          // call once per loop
  bool pressed() const { return pressedEdge_; }    // finger just landed (once per touch)
  bool released() const { return releasedEdge_; }
  bool down() const { return down_ && !consumed_; }
  int16_t x() const { return x_; }
  int16_t y() const { return y_; }
  // Ignore the rest of the current touch (so the tap that opened a screen isn't also used on it).
  void consume() { if (down_) consumed_ = true; pressedEdge_ = false; }
  bool rawPoint(int &rx, int &ry);      // for calibration
  void applyCalibration();

 private:
  bool down_ = false, pressedEdge_ = false, releasedEdge_ = false, consumed_ = false;
  uint8_t stable_ = 0;
  uint32_t lastSeen_ = 0;
  int16_t x_ = 0, y_ = 0;
  int16_t xMin_ = TOUCH_RAW_X_MIN, xMax_ = TOUCH_RAW_X_MAX, yMin_ = TOUCH_RAW_Y_MIN, yMax_ = TOUCH_RAW_Y_MAX;
};
extern Touch touch;
void touchCalibrate();   // interactive 4-corner calibration, saves to settings

// ---------------- backlight ----------------
namespace backlight {
void begin();
void set(uint8_t level);      // 0..255 raw
void update();                // call each loop; handles auto-dim
}

// ---------------- sound ----------------
namespace sound {
void begin();
void update();
void click();
void good();     // positive blip
void bad();      // negative buzz
void eat();
void win();
void lose();
}

// ---------------- RGB LED (active low on the CYD) ----------------
namespace led {
void begin();
void flash(bool r, bool g, bool b, uint16_t ms = 400);
void update();
}
