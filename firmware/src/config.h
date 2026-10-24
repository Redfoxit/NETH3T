#pragma once
// Board + app configuration for the ESP32-2432S028R (CYD).
#include <Arduino.h>

#define APP_NAME      "NETH3T"
#define APP_VERSION   "0.1.0"
#define HOSTNAME      "neth3t"
#define OTA_PASSWORD  "cyd-games"          // change me (and `upload_flags` in platformio.ini)
#define SETUP_AP_NAME "NETH3T-Setup"       // WiFi network the CYD creates for first-time setup

// ---- Screen (landscape) ----
#define SCREEN_W 320
#define SCREEN_H 240
#define SCREEN_ROTATION 1                  // 1 = landscape, USB port on the left side
#define INVERT_DISPLAY false               // flip to true if colours look inverted

// ---- Pins ----
#define PIN_BACKLIGHT 21
#define PIN_SPEAKER   26                   // driven through the on-board amp
#define PIN_LDR       34                   // light sensor (input only)
#define PIN_LED_R     4                    // RGB LED, active LOW
#define PIN_LED_G     16
#define PIN_LED_B     17
#define LDR_DARK_IS_HIGH true              // flip if auto-dim goes the wrong way

// Touch controller (XPT2046) sits on its own SPI bus
#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_CLK  25
#define TOUCH_MOSI 32
#define TOUCH_MISO 39

// Default raw->pixel calibration (works on most units; Settings > Calibrate touch overrides it)
#define TOUCH_RAW_X_MIN 200
#define TOUCH_RAW_X_MAX 3700
#define TOUCH_RAW_Y_MIN 240
#define TOUCH_RAW_Y_MAX 3800

// ---- Time ----
// POSIX TZ string for Melbourne (AEST/AEDT). Used for the Wordle daily word.
#define TZ_STRING "AEST-10AEDT,M10.1.0,M4.1.0/3"
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.cloudflare.com"

// ---- Wordle ----
// Day number 0 = this date; used to pick the daily word (same maths as the server).
#define WORDLE_EPOCH_Y 2021
#define WORDLE_EPOCH_M 6
#define WORDLE_EPOCH_D 19
// Default word-server URL, editable from the WiFi setup portal. Example: http://192.168.1.50:8088
#define DEFAULT_WORD_SERVER ""

// ---- Colours (RGB565) ----
#define C_BG        0x0000
#define C_PANEL     0x18E3
#define C_PANEL_HI  0x39E7
#define C_TEXT      0xFFFF
#define C_MUTED     0x8C71
#define C_ACCENT    0xFD20   // orange
#define C_GOOD      0x2E89   // green
#define C_WARN      0xFE60   // amber
#define C_BAD       0xC8A2   // red
#define C_X         0x5D7F   // light blue
#define C_O         0xFBE0   // yellow-orange
