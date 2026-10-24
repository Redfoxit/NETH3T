# NETH3T games firmware (CYD build)

Noughts & crosses, Snake and Wordle for the **ESP32-2432S028R "Cheap Yellow Display"** (2.8" 320x240 ILI9341 TFT,
XPT2046 resistive touch), plus an optional word/stats server.

This is a CYD-targeted build: the three games are each an `App` on a simple tile menu, built on an extensible
`App` framework (`enter()` / `tick()` / `done`) so more tools can be added later as additional apps.

```
NETH3T/
  firmware/        PlatformIO project (open this folder in VS Code)
  wordle-server/   Python server + Dockerfile (optional, for shared daily word + stats)
  tools/           word-list generator
```

## Status

- **Builds and flashes on real hardware.** `pio run -e cyd` compiles clean (Flash ~91% of the OTA app slot), uploads
  over USB, and boots to the WiFi setup portal on an ESP32-2432S028R.
- Game logic (noughts & crosses AI, Snake rules, Wordle scoring, daily-word maths, word tables) and the Python server
  are verified natively; the Python server and the firmware pick the same daily word.
- **Not yet play-tested on the screen:** display output, touch accuracy and the feel of each game still need a look on
  the board. Expect to calibrate touch and possibly flip a colour/panel setting on first boot (see Troubleshooting).

## 1. Build and flash (VS Code)

1. Install the **PlatformIO IDE** extension in VS Code.
2. `File > Open Folder...` and choose `NETH3T/firmware`. PlatformIO downloads the toolchain and libraries.
3. Plug the CYD in by USB, then click the PlatformIO **Upload** arrow (or `pio run -e cyd -t upload`).
4. Watch the output with the serial **Monitor** (115200 baud) if something misbehaves.

If the screen stays blank, garbled, or the colours look inverted, your board is probably the variant with an ST7789
panel (often the one with both micro-USB and USB-C). Upload with `pio run -e cyd_st7789 -t upload` instead.

> Library note: the PlatformIO registry snapshot of `XPT2046_Touchscreen` lacks `begin(SPIClass&)`, so `platformio.ini`
> pins the upstream v1.4 release by URL. Leave that pin in place.

### First boot

The CYD shows "WiFi setup". Join the `NETH3T-Setup` network from your phone, open `192.168.4.1`, pick your WiFi
(2.4GHz only) and optionally enter the word-server URL. After that it remembers both. The same screen is under
**Settings > WiFi / server setup**.

### Wireless updates

Once it is on your WiFi: `pio run -e cyd_ota -t upload`. The device advertises as `neth3t.local`. The OTA password is
the placeholder `cyd-games`; change it in `src/config.h` (`OTA_PASSWORD`) **and** `upload_flags` in `platformio.ini`.

## 2. Games

- **Noughts & Crosses:** Easy, Normal, Unbeatable (it cannot be beaten, only drawn) or two players. Who goes first
  alternates each round.
- **Snake:** tap the side of the screen you want to turn towards. The screen is split into four triangles by its
  diagonals. Two turns can be queued, the snake speeds up as it grows, and your best score is saved.
- **Wordle:** Daily word or Random. The whole word list lives in the firmware (about 1,400 everyday answers, 14,855
  accepted guesses), so it works with no network. The daily word needs the clock, which comes from NTP over WiFi.
- **Settings:** brightness, sound, light-sensor auto-dim, touch calibration, WiFi/server setup.

The sound, RGB LED and light sensor on the board are all used (win/lose jingles and LED flashes; auto-dim).

## 3. Word server (optional)

The server adds: the shared daily word (the same word on every device), win/streak tracking per player, and a web
dashboard at `http://<host-ip>:8088/`. The firmware works fine without it.

```
cd wordle-server
docker compose up -d --build
```

Then put `http://<host-ip>:8088` in the device's setup portal. `https://` URLs work too (the device skips certificate
checks, which is fine for a LAN toy). The server never sends the word to the device, only an index into the shared
word table. The device reports results under the player name `neth3t` (its hostname).

## 4. Changing things

- **Word lists:** edit `EXTRA_EXCLUDE` in `tools/gen_words.py` and run
  `pip install wordfreq better-profanity && python3 tools/gen_words.py`. That rewrites `firmware/src/words.h` and the
  server's word files together, so re-flash and rebuild the server afterwards so they stay in step.
- **Timezone:** `TZ_STRING` in `src/config.h` (Melbourne by default) and `TZ_NAME` for the server.
- **Pins and calibration defaults:** `src/config.h`. Touch is recalibrated from the Settings screen.
- **Auto-dim going the wrong way:** flip `LDR_DARK_IS_HIGH` in `config.h`.
- **Adding an app/game:** write a class derived from `App` (see `src/app.h` and `app_snake.cpp`), add a `makeXxx()`
  factory, and add a tile in `main.cpp`.

## 5. Troubleshooting

- **Touch is offset or mirrored:** Settings > Calibrate touch.
- **Touch does nothing but the display works:** the display is set to HSPI (`-DUSE_HSPI_PORT`) and touch uses VSPI.
  If your board differs, try removing that flag and changing `SPIClass touchSPI(VSPI)` to `HSPI` in `hal.cpp`.
- **Colours inverted:** set `INVERT_DISPLAY` to `true` in `config.h`.
- **Speaker hiss:** normal for the on-board amp. Turn sound off in Settings.
- **Daily says "Clock not set":** it needs WiFi for a few seconds to fetch the time.

## 6. Run the tests

    g++ -std=c++17 -O1 firmware/test/host_tests.cpp -o host_tests && ./host_tests
    cd wordle-server && python3 test_server.py

Expected: the host tests report `0 failures` and the server tests print `server tests: ALL OK`.
