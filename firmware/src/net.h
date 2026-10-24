#pragma once
// WiFi, NTP, OTA and the (optional) Unraid word-server client.
#include <Arduino.h>

namespace net {

void begin();                 // connect using saved credentials, or run the setup portal on first boot
void loop();                  // call every loop: OTA + housekeeping
bool connected();
String ip();
void runPortal();             // WiFi + server-URL setup portal on demand (blocks until done/timeout)
void forgetWifi();            // erase saved WiFi credentials

bool timeValid();
bool localDate(int &y, int &m, int &d);

bool serverConfigured();

// Daily word info from the server. `index` is an index into the firmware's answer table.
struct DailyInfo {
  bool ok = false;
  int index = 0;
  int day = 0;
  int answers = 0;     // the server's answer-list size, used to detect mismatched word lists
  String date;
};
DailyInfo fetchDaily();

struct Stats {
  bool ok = false;
  int played = 0, wins = 0, streak = 0, maxStreak = 0;
};
// Reports a finished game and returns the updated stats (ok=false if the server is unreachable).
Stats reportResult(bool daily, int day, const char *word, bool won, int guesses);

}  // namespace net
