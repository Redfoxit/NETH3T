// WiFi Tools: a hidden screen reached by long-pressing the title on the menu.
// Passive, read-only recon only - it lists nearby access points, 2.4GHz channel
// usage and this device's own connection. Scans are passive (listen for beacons);
// no frames are transmitted at other devices, and there is no attack functionality.
#include <WiFi.h>

#include "app.h"
#include "ui.h"

namespace {

const char *authStr(wifi_auth_mode_t m) {
  switch (m) {
    case WIFI_AUTH_OPEN:            return "open";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-E";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/3";
    default:                        return "?";
  }
}

const Rect TAB_SCAN = {2, 30, 102, 26};
const Rect TAB_CHAN = {109, 30, 102, 26};
const Rect TAB_DEV  = {216, 30, 102, 26};
const Rect B_RESCAN = {2, 214, 110, 24};
const Rect B_NEXT   = {208, 214, 110, 24};

enum View { V_SCAN, V_CHAN, V_DEV };

class WifiToolsApp : public App {
 public:
  void enter() override {
    done = false;
    view_ = V_SCAN;
    page_ = 0;
    scan();
    touch.consume();
  }

  void tick() override {
    if (backTapped()) { done = true; return; }
    if (!touch.pressed()) return;
    int x = touch.x(), y = touch.y();
    if (TAB_SCAN.hit(x, y))      { view_ = V_SCAN; page_ = 0; sound::click(); draw(); }
    else if (TAB_CHAN.hit(x, y)) { view_ = V_CHAN; sound::click(); draw(); }
    else if (TAB_DEV.hit(x, y))  { view_ = V_DEV;  sound::click(); draw(); }
    else if (view_ == V_SCAN && B_RESCAN.hit(x, y)) { sound::click(); scan(); }
    else if (view_ == V_SCAN && B_NEXT.hit(x, y) && pages() > 1) {
      page_ = (page_ + 1) % pages();
      sound::click();
      draw();
    }
  }

 private:
  static const int ROWS = 7;
  int count_ = 0, view_ = V_SCAN, page_ = 0;

  int pages() const { return count_ <= 0 ? 1 : (count_ + ROWS - 1) / ROWS; }

  void scan() {
    tft.fillScreen(C_BG);
    drawTabs();
    drawText("Scanning...", SCREEN_W / 2, 130, 4, C_ACCENT, C_BG, MC_DATUM);
    int n = WiFi.scanNetworks(false /*sync*/, true /*show hidden*/, true /*passive*/);
    count_ = n < 0 ? 0 : n;
    page_ = 0;
    draw();
  }

  void drawTabs() {
    tft.fillRect(0, 0, SCREEN_W, 26, C_PANEL);
    drawButton(Rect{2, 2, 72, 22}, "< Menu", C_PANEL_HI);
    drawText("WiFi Tools", SCREEN_W / 2 + 30, 13, 2, C_TEXT, C_PANEL, MC_DATUM);
    drawButton(TAB_SCAN, "Scan",     view_ == V_SCAN ? C_ACCENT : C_PANEL_HI);
    drawButton(TAB_CHAN, "Channels", view_ == V_CHAN ? C_ACCENT : C_PANEL_HI);
    drawButton(TAB_DEV,  "Device",   view_ == V_DEV  ? C_ACCENT : C_PANEL_HI);
  }

  void draw() {
    drawTabs();
    tft.fillRect(0, 60, SCREEN_W, 180, C_BG);   // content + footer
    if (view_ == V_SCAN) drawScan();
    else if (view_ == V_CHAN) drawChannels();
    else drawDevice();
  }

  void drawScan() {
    if (count_ == 0) {
      drawText("No networks found", SCREEN_W / 2, 120, 2, C_MUTED, C_BG, MC_DATUM);
    } else {
      for (int i = 0; i < ROWS; i++) {
        int idx = page_ * ROWS + i;
        if (idx >= count_) break;
        int y = 72 + i * 20;
        String s = WiFi.SSID(idx);
        if (s.length() == 0) s = "(hidden)";
        char ssid[20];
        snprintf(ssid, sizeof(ssid), "%s", s.c_str());
        drawText(ssid, 6, y, 2, C_TEXT, C_BG, ML_DATUM);
        char meta[32];
        snprintf(meta, sizeof(meta), "ch%d  %ddBm  %s",
                 WiFi.channel(idx), WiFi.RSSI(idx), authStr(WiFi.encryptionType(idx)));
        drawText(meta, 314, y, 1, C_MUTED, C_BG, MR_DATUM);
      }
    }
    char foot[24];
    snprintf(foot, sizeof(foot), "%d nets  p%d/%d", count_, page_ + 1, pages());
    drawText(foot, SCREEN_W / 2, 226, 1, C_MUTED, C_BG, MC_DATUM);
    drawButton(B_RESCAN, "Rescan", C_PANEL_HI);
    if (pages() > 1) drawButton(B_NEXT, "Next >", C_PANEL_HI);
  }

  void drawChannels() {
    int cnt[14] = {0};
    int best[14];
    for (int c = 0; c < 14; c++) best[c] = -200;
    for (int i = 0; i < count_; i++) {
      int c = WiFi.channel(i);
      if (c < 1 || c > 13) continue;
      cnt[c]++;
      int r = WiFi.RSSI(i);
      if (r > best[c]) best[c] = r;
    }
    for (int c = 1; c <= 13; c++) {
      int y = 64 + (c - 1) * 11;
      char lbl[4];
      snprintf(lbl, sizeof(lbl), "%2d", c);
      drawText(lbl, 4, y, 1, C_TEXT, C_BG, TL_DATUM);
      int bw = cnt[c] * 16;
      if (bw > 228) bw = 228;
      if (bw > 0) tft.fillRect(26, y, bw, 8, C_X);
      if (cnt[c] > 0) {
        char t[20];
        snprintf(t, sizeof(t), "%d  %ddBm", cnt[c], best[c]);
        drawText(t, 260, y, 1, C_MUTED, C_BG, TL_DATUM);
      }
    }
    drawText("2.4GHz APs per channel", SCREEN_W / 2, 226, 1, C_MUTED, C_BG, MC_DATUM);
  }

  void drawDevice() {
    if (WiFi.status() != WL_CONNECTED) {
      drawText("Not connected to WiFi", SCREEN_W / 2, 120, 2, C_MUTED, C_BG, MC_DATUM);
      return;
    }
    char line[48];
    int y = 70;
    drawDevRow("SSID:", WiFi.SSID(), line, y);
    drawDevRow("IP:",   WiFi.localIP().toString(), line, y);
    drawDevRow("GW:",   WiFi.gatewayIP().toString(), line, y);
    drawDevRow("DNS:",  WiFi.dnsIP().toString(), line, y);
    drawDevRow("MAC:",  WiFi.macAddress(), line, y);
    snprintf(line, sizeof(line), "RSSI: %ddBm   ch%d", WiFi.RSSI(), WiFi.channel());
    drawText(line, 10, y, 2, C_TEXT, C_BG, ML_DATUM);
  }

  void drawDevRow(const char *key, const String &val, char *buf, int &y) {
    snprintf(buf, 48, "%s %s", key, val.c_str());
    drawText(buf, 10, y, 2, C_TEXT, C_BG, ML_DATUM);
    y += 24;
  }
};

}  // namespace

App *makeWifiTools() {
  static WifiToolsApp app;
  return &app;
}
