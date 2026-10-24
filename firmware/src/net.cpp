#include "net.h"

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <time.h>

#include "config.h"
#include "hal.h"
#include "ui.h"

namespace net {

static bool timeStarted = false;
static bool otaStarted = false;

static void startTime() {
  configTzTime(TZ_STRING, NTP_SERVER_1, NTP_SERVER_2);
  timeStarted = true;
}

static void startOta() {
  if (otaStarted) return;
  otaStarted = true;
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    centerMessage("Updating firmware...", "Do not power off");
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    int w = (int)((uint64_t)done * 240 / total);
    tft.drawRect(40, 170, 240, 12, C_TEXT);
    tft.fillRect(40, 170, w, 12, C_GOOD);
  });
  ArduinoOTA.onEnd([]() { centerMessage("Update complete", "Restarting..."); });
  ArduinoOTA.onError([](ota_error_t e) {
    char buf[24];
    snprintf(buf, sizeof(buf), "OTA error %d", (int)e);
    centerMessage("Update failed", buf);
    delay(2000);
  });
  ArduinoOTA.begin();
}

static void showPortalHelp(WiFiManager *) {
  tft.fillScreen(C_BG);
  drawText("WiFi setup", SCREEN_W / 2, 30, 4, C_ACCENT, C_BG, MC_DATUM);
  drawText("1. On your phone, join the WiFi:", 16, 70, 2, C_TEXT, C_BG);
  drawText(SETUP_AP_NAME, 32, 92, 4, C_TEXT, C_BG);
  drawText("2. Open  192.168.4.1  in a browser", 16, 130, 2, C_TEXT, C_BG);
  drawText("3. Pick your WiFi and, if you have", 16, 154, 2, C_TEXT, C_BG);
  drawText("   one, enter the word-server URL", 16, 172, 2, C_TEXT, C_BG);
  drawText("(2.4GHz networks only)", 16, 204, 2, C_MUTED, C_BG);
}

void runPortal() {
  WiFiManager wm;
  WiFiManagerParameter serverParam("server", "Word server URL (optional, e.g. http://192.168.1.50:8088)",
                                   settings.serverUrl.c_str(), 120);
  wm.addParameter(&serverParam);
  wm.setHostname(HOSTNAME);
  wm.setAPCallback(showPortalHelp);
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(20);
  showPortalHelp(nullptr);
  bool ok = wm.startConfigPortal(SETUP_AP_NAME);
  String url = serverParam.getValue();
  url.trim();
  while (url.endsWith("/")) url.remove(url.length() - 1);
  if (url != settings.serverUrl) {
    settings.serverUrl = url;
    settingsSave();
  }
  if (ok || WiFi.status() == WL_CONNECTED) {
    if (!timeStarted) startTime();
    startOta();
  }
}

void begin() {
  WiFi.persistent(true);   // make sure saved credentials get written to NVS
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);

  // Ask the stored config directly whether we have creds. WiFi.SSID() is empty
  // until a connection completes, so it can't be used to detect saved creds.
  WiFiManager wm;
  bool haveCreds = wm.getWiFiIsSaved();
  if (haveCreds) {
    WiFi.begin();   // uses credentials saved in flash
    centerMessage("Connecting to WiFi...", wm.getWiFiSSID().c_str());
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(100);
  } else {
    runPortal();   // first boot: nothing saved yet
  }
  if (WiFi.status() == WL_CONNECTED) {
    startTime();
    startOta();
    Serial.printf("WiFi connected, IP %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi not connected: running offline (games still work)");
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!timeStarted) startTime();
    startOta();
    ArduinoOTA.handle();
  }
}

bool connected() { return WiFi.status() == WL_CONNECTED; }
String ip() { return WiFi.localIP().toString(); }

void forgetWifi() {
  WiFi.disconnect(true, true);   // erase saved credentials
}

bool timeValid() { return time(nullptr) > 1735689600; }   // after 2025-01-01

bool localDate(int &y, int &m, int &d) {
  if (!timeValid()) return false;
  struct tm t;
  if (!getLocalTime(&t, 0)) return false;
  y = t.tm_year + 1900;
  m = t.tm_mon + 1;
  d = t.tm_mday;
  return true;
}

bool serverConfigured() { return settings.serverUrl.length() > 8; }

// ---- tiny HTTP helper (http:// or https://; https skips certificate checks, fine for a LAN toy) ----
static bool httpJson(const String &path, const char *method, const String &body, JsonDocument &out) {
  if (!connected() || !serverConfigured()) return false;
  String url = settings.serverUrl + path;
  HTTPClient http;
  WiFiClient plain;
  WiFiClientSecure secure;
  bool ok = false;
  if (url.startsWith("https://")) {
    secure.setInsecure();
    ok = http.begin(secure, url);
  } else {
    ok = http.begin(plain, url);
  }
  if (!ok) return false;
  http.setConnectTimeout(2500);
  http.setTimeout(3500);
  int code;
  if (!strcmp(method, "POST")) {
    http.addHeader("Content-Type", "application/json");
    code = http.POST(body);
  } else {
    code = http.GET();
  }
  bool good = false;
  if (code == 200) {
    DeserializationError err = deserializeJson(out, http.getString());
    good = !err;
  }
  http.end();
  return good;
}

DailyInfo fetchDaily() {
  DailyInfo info;
  JsonDocument doc;
  if (!httpJson("/api/today", "GET", "", doc)) return info;
  info.index = doc["index"] | -1;
  info.day = doc["day"] | 0;
  info.answers = doc["answers"] | 0;
  info.date = doc["date"] | "";
  info.ok = info.index >= 0;
  return info;
}

Stats reportResult(bool daily, int day, const char *word, bool won, int guesses) {
  Stats s;
  JsonDocument req;
  req["player"] = HOSTNAME;
  req["mode"] = daily ? "daily" : "random";
  req["day"] = day;
  req["word"] = word;
  req["won"] = won;
  req["guesses"] = guesses;
  String body;
  serializeJson(req, body);
  JsonDocument doc;
  if (!httpJson("/api/result", "POST", body, doc)) return s;
  s.played = doc["played"] | 0;
  s.wins = doc["wins"] | 0;
  s.streak = doc["streak"] | 0;
  s.maxStreak = doc["max_streak"] | 0;
  s.ok = true;
  return s;
}

}  // namespace net
