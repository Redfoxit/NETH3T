// NETH3T games - noughts & crosses, Snake and Wordle for the ESP32-2432S028R.
#include <Arduino.h>

#include "app.h"
#include "config.h"
#include "hal.h"
#include "net.h"
#include "ui.h"

namespace {

struct Tile {
  const char *title;
  const char *subtitle;
  uint16_t colour;
  App *(*make)();
  Rect r;
};

Tile tiles[4] = {
    {"Noughts & Crosses", "vs CYD or 2 players", 0x2C6B, makeTicTacToe, {8, 36, 148, 88}},
    {"Snake", "tap to turn", 0x2E89, makeSnake, {164, 36, 148, 88}},
    {"Wordle", "daily + random", 0xB5A0, makeWordle, {8, 132, 148, 88}},
    {"Settings", "display, WiFi, sound", 0x4A69, makeSettings, {164, 132, 148, 88}},
};

App *current = nullptr;
bool lastWifi = false;

void drawIcon(int i) {
  const Rect &r = tiles[i].r;
  int cx = r.cx(), top = r.y + 8;
  switch (i) {
    case 0:  // # grid with an X and an O
      for (int k = 1; k < 3; k++) {
        tft.fillRect(cx - 21 + k * 14 - 1, top, 2, 42, C_TEXT);
        tft.fillRect(cx - 21, top + k * 14 - 1, 42, 2, C_TEXT);
      }
      tft.drawLine(cx - 19, top + 2, cx - 9, top + 12, C_X);
      tft.drawLine(cx - 9, top + 2, cx - 19, top + 12, C_X);
      tft.drawCircle(cx + 14, top + 35, 5, C_O);
      break;
    case 1:  // zig-zag snake
      for (int k = 0; k < 6; k++) tft.fillRect(cx - 30 + k * 10, top + 10 + ((k % 2) ? 12 : 0) + 6, 9, 9, k == 5 ? 0x7FE0 : C_TEXT);
      tft.fillCircle(cx + 32, top + 8, 4, 0xF800);
      break;
    case 2:  // five wordle tiles
      for (int k = 0; k < 5; k++) {
        uint16_t c = (k == 1 || k == 4) ? 0x3E04 : (k == 2) ? 0xCD40 : 0x39E7;
        tft.fillRect(cx - 40 + k * 17, top + 12, 15, 15, c);
      }
      break;
    default:  // sliders
      for (int k = 0; k < 3; k++) {
        tft.fillRect(cx - 28, top + 8 + k * 14, 56, 3, C_TEXT);
        tft.fillCircle(cx - 14 + k * 14, top + 9 + k * 14, 5, C_ACCENT);
      }
      break;
  }
}

void drawStatus() {
  tft.fillRect(150, 0, 170, 30, C_PANEL);
  if (net::connected()) drawText("WiFi online", 312, 15, 2, C_GOOD, C_PANEL, MR_DATUM);
  else drawText("offline", 312, 15, 2, C_WARN, C_PANEL, MR_DATUM);
  lastWifi = net::connected();
}

void drawMenu() {
  tft.fillScreen(C_BG);
  tft.fillRect(0, 0, SCREEN_W, 30, C_PANEL);
  drawText(APP_NAME, 10, 15, 4, C_ACCENT, C_PANEL, ML_DATUM);
  drawStatus();
  for (int i = 0; i < 4; i++) {
    const Tile &t = tiles[i];
    tft.fillRoundRect(t.r.x, t.r.y, t.r.w, t.r.h, 10, t.colour);
    drawIcon(i);
    drawText(t.title, t.r.cx(), t.r.y + 64, 2, C_TEXT, t.colour, MC_DATUM);
    drawText(t.subtitle, t.r.cx(), t.r.y + 79, 1, C_TEXT, t.colour, MC_DATUM);
  }
  drawText("Tap a game to play", SCREEN_W / 2, 230, 2, C_MUTED, C_BG, MC_DATUM);
}

void menuTick() {
  if (net::connected() != lastWifi) drawStatus();
  if (!touch.pressed()) return;
  for (int i = 0; i < 4; i++) {
    if (!tiles[i].r.hit(touch.x(), touch.y())) continue;
    sound::click();
    current = tiles[i].make();
    current->enter();
    return;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  settingsLoad();
  led::begin();
  sound::begin();
  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  tft.invertDisplay(INVERT_DISPLAY);
  tft.fillScreen(C_BG);
  backlight::begin();
  touch.begin();

  drawText(APP_NAME, SCREEN_W / 2, 105, 4, C_ACCENT, C_BG, MC_DATUM);
  drawText("v" APP_VERSION, SCREEN_W / 2, 140, 2, C_MUTED, C_BG, MC_DATUM);

  net::begin();
  drawMenu();
}

void loop() {
  touch.poll();
  sound::update();
  led::update();
  backlight::update();
  net::loop();

  if (current) {
    current->tick();
    if (current->done) {
      current = nullptr;
      drawMenu();
      touch.consume();
    }
  } else {
    menuTick();
  }
  delay(1);
}
