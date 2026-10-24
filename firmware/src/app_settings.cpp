// Settings: brightness, sound, auto-dim, touch calibration, WiFi / server setup.
#include "app.h"
#include "net.h"
#include "ui.h"

namespace {

const Rect B_MINUS = {200, 32, 50, 30}, B_PLUS = {260, 32, 50, 30};
const Rect B_SOUND = {200, 70, 110, 30}, B_AUTO = {200, 106, 110, 30};
const Rect B_CAL = {10, 146, 148, 34}, B_WIFI = {162, 146, 148, 34};
const Rect B_FORGET = {10, 186, 148, 30}, B_REBOOT = {162, 186, 148, 30};

class SettingsApp : public App {
 public:
  void enter() override {
    done = false;
    draw();
    touch.consume();
  }

  void tick() override {
    if (backTapped()) { done = true; return; }
    if (!touch.pressed()) return;
    int x = touch.x(), y = touch.y();

    if (B_MINUS.hit(x, y) || B_PLUS.hit(x, y)) {
      int v = settings.brightness + (B_PLUS.hit(x, y) ? 20 : -20);
      settings.brightness = (uint8_t)constrain(v, 20, 255);
      backlight::set(settings.brightness);
      settingsSave();
      sound::click();
      drawBrightness();
    } else if (B_SOUND.hit(x, y)) {
      settings.sound = !settings.sound;
      settingsSave();
      sound::click();
      drawToggles();
    } else if (B_AUTO.hit(x, y)) {
      settings.autoDim = !settings.autoDim;
      settingsSave();
      sound::click();
      drawToggles();
    } else if (B_CAL.hit(x, y)) {
      touchCalibrate();
      draw();
      touch.consume();
    } else if (B_WIFI.hit(x, y)) {
      net::runPortal();
      draw();
      touch.consume();
    } else if (B_FORGET.hit(x, y)) {
      net::forgetWifi();
      centerMessage("WiFi forgotten", "Restarting...");
      delay(1500);
      ESP.restart();
    } else if (B_REBOOT.hit(x, y)) {
      centerMessage("Restarting...");
      delay(500);
      ESP.restart();
    }
  }

 private:
  void drawBrightness() {
    tft.fillRect(110, 32, 80, 30, C_BG);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", (settings.brightness * 100) / 255);
    drawText(buf, 150, 47, 2, C_TEXT, C_BG, MC_DATUM);
  }

  void drawToggles() {
    drawButton(B_SOUND, settings.sound ? "ON" : "OFF", settings.sound ? C_GOOD : C_PANEL_HI);
    drawButton(B_AUTO, settings.autoDim ? "ON" : "OFF", settings.autoDim ? C_GOOD : C_PANEL_HI);
  }

  void draw() {
    tft.fillScreen(C_BG);
    drawHeader("Settings");
    drawText("Brightness", 12, 39, 2, C_TEXT, C_BG);
    drawButton(B_MINUS, "-", C_PANEL_HI, C_TEXT, 4);
    drawButton(B_PLUS, "+", C_PANEL_HI, C_TEXT, 4);
    drawBrightness();
    drawText("Sound", 12, 77, 2, C_TEXT, C_BG);
    drawText("Auto-dim (light sensor)", 12, 113, 2, C_TEXT, C_BG);
    drawToggles();
    drawButton(B_CAL, "Calibrate touch", C_PANEL_HI);
    drawButton(B_WIFI, "WiFi / server setup", C_PANEL_HI);
    drawButton(B_FORGET, "Forget WiFi", C_PANEL_HI);
    drawButton(B_REBOOT, "Restart", C_PANEL_HI);
    char buf[64];
    if (net::connected()) snprintf(buf, sizeof(buf), "%s  v%s  server %s", net::ip().c_str(), APP_VERSION, net::serverConfigured() ? "set" : "off");
    else snprintf(buf, sizeof(buf), "offline  v%s", APP_VERSION);
    drawText(buf, SCREEN_W / 2, 228, 2, C_MUTED, C_BG, MC_DATUM);
  }
};

}  // namespace

App *makeSettings() {
  static SettingsApp app;
  return &app;
}
