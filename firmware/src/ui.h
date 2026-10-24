#pragma once
// Small drawing / hit-testing helpers shared by all screens.
#include "hal.h"

struct Rect {
  int16_t x, y, w, h;
  bool hit(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
  int16_t cx() const { return x + w / 2; }
  int16_t cy() const { return y + h / 2; }
};

static const Rect BACK_RECT = {0, 0, 78, 26};

inline void drawText(const char *s, int x, int y, uint8_t font, uint16_t fg, uint16_t bg, uint8_t datum = TL_DATUM) {
  tft.setTextColor(fg, bg);
  tft.setTextDatum(datum);
  tft.drawString(s, x, y, font);
  tft.setTextDatum(TL_DATUM);
}

inline void drawButton(const Rect &r, const char *label, uint16_t fill, uint16_t fg = C_TEXT, uint8_t font = 2) {
  tft.fillRoundRect(r.x, r.y, r.w, r.h, 6, fill);
  drawText(label, r.cx(), r.cy(), font, fg, fill, MC_DATUM);
}

// Top bar with a "< Menu" button on the left and a centred title.
inline void drawHeader(const char *title) {
  tft.fillRect(0, 0, SCREEN_W, 26, C_PANEL);
  drawButton(Rect{2, 2, 72, 22}, "< Menu", C_PANEL_HI);
  drawText(title, SCREEN_W / 2 + 30, 13, 2, C_TEXT, C_PANEL, MC_DATUM);
}

inline bool backTapped() { return touch.pressed() && BACK_RECT.hit(touch.x(), touch.y()); }

// Clears a rectangle and prints (up to 4) lines of text, split on '\n'.
inline void drawLines(const Rect &area, const char *text, uint8_t font, uint16_t fg, uint16_t bg, int lineHeight) {
  tft.fillRect(area.x, area.y, area.w, area.h, bg);
  tft.setTextColor(fg, bg);
  tft.setTextDatum(TL_DATUM);
  char buf[96];
  strncpy(buf, text, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  int y = area.y;
  for (char *line = strtok(buf, "\n"); line && y + lineHeight <= area.y + area.h; line = strtok(nullptr, "\n")) {
    tft.drawString(line, area.x, y, font);
    y += lineHeight;
  }
}

inline void centerMessage(const char *l1, const char *l2 = nullptr) {
  tft.fillScreen(C_BG);
  drawText(l1, SCREEN_W / 2, l2 ? 105 : 120, 4, C_TEXT, C_BG, MC_DATUM);
  if (l2) drawText(l2, SCREEN_W / 2, 140, 2, C_MUTED, C_BG, MC_DATUM);
}
