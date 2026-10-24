// Snake. Tap the part of the screen in the direction you want to turn: the screen is split into
// four triangles by its diagonals (up / down / left / right), which suits a resistive touchscreen.
#include "app.h"
#include "logic/snake_logic.h"
#include "ui.h"

namespace {

const int CELL = 12;
const int GW = 26, GH = 17;                 // 312 x 204 px playfield
const int X0 = 4, Y0 = 31;                  // below the 26px header, centred
const uint16_t C_HEAD = 0x7FE0, C_BODY = 0x2E89, C_FOOD = 0xF800;
const char *HS_KEY = "hs_snake";

class SnakeApp : public App {
 public:
  void enter() override {
    done = false;
    best_ = prefGetU32(HS_KEY, 0);
    game_.reset(esp_random());
    state_ = READY;
    drawAll();
    drawOverlay("Tap to start", "tap the side you want to turn");
    touch.consume();
  }

  void tick() override {
    if (backTapped()) { done = true; return; }

    if (state_ == READY) {
      if (touch.pressed()) {
        drawAll();   // clears the overlay
        state_ = RUN;
        lastStep_ = millis();
        sound::click();
        touch.consume();
      }
      return;
    }

    if (state_ == OVER) {
      if (touch.pressed() && millis() - overAt_ > 500) {
        game_.reset(esp_random());
        drawAll();
        state_ = RUN;
        lastStep_ = millis();
        touch.consume();
      }
      return;
    }

    if (touch.pressed()) {
      if (game_.turn(dirFromTap(touch.x(), touch.y()))) sound::click();
    }
    uint32_t interval = 150 - (uint32_t)min(game_.score() * 3, 85);
    if (millis() - lastStep_ >= interval) {
      lastStep_ = millis();
      stepOnce();
    }
  }

 private:
  enum State { READY, RUN, OVER };
  State state_ = READY;
  snake::Game<GW, GH> game_;
  uint32_t lastStep_ = 0, overAt_ = 0, best_ = 0;
  snake::Pt prevHead_{0, 0};

  static snake::Dir dirFromTap(int x, int y) {
    int dx = x - SCREEN_W / 2, dy = y - SCREEN_H / 2;
    // compare against the screen's own diagonals so the four zones are equal triangles
    if (abs(dx) * SCREEN_H > abs(dy) * SCREEN_W) return dx > 0 ? snake::RIGHT : snake::LEFT;
    return dy > 0 ? snake::DOWN : snake::UP;
  }

  void fillCell(snake::Pt p, uint16_t colour) {
    tft.fillRect(X0 + p.x * CELL, Y0 + p.y * CELL, CELL, CELL, C_BG);
    if (colour != C_BG) tft.fillRect(X0 + p.x * CELL + 1, Y0 + p.y * CELL + 1, CELL - 2, CELL - 2, colour);
  }

  void drawFood() {
    snake::Pt f = game_.food();
    fillCell(f, C_BG);
    tft.fillCircle(X0 + f.x * CELL + CELL / 2, Y0 + f.y * CELL + CELL / 2, 4, C_FOOD);
  }

  void drawScore() {
    tft.fillRect(150, 0, 170, 26, C_PANEL);
    char buf[32];
    snprintf(buf, sizeof(buf), "Score %d   Best %u", game_.score(), (unsigned)best_);
    drawText(buf, 316, 13, 2, C_TEXT, C_PANEL, MR_DATUM);
  }

  void drawAll() {
    tft.fillScreen(C_BG);
    tft.fillRect(0, 0, SCREEN_W, 26, C_PANEL);
    drawButton(Rect{2, 2, 72, 22}, "< Menu", C_PANEL_HI);
    drawScore();
    tft.drawRect(X0 - 1, Y0 - 1, GW * CELL + 2, GH * CELL + 2, C_PANEL_HI);
    for (int i = game_.length() - 1; i >= 1; i--) fillCell(game_.segment(i), C_BODY);
    fillCell(game_.segment(0), C_HEAD);
    prevHead_ = game_.segment(0);
    drawFood();
  }

  void drawOverlay(const char *l1, const char *l2) {
    tft.fillRoundRect(50, 85, 220, 76, 8, C_PANEL);
    tft.drawRoundRect(50, 85, 220, 76, 8, C_ACCENT);
    drawText(l1, SCREEN_W / 2, 110, 4, C_TEXT, C_PANEL, MC_DATUM);
    drawText(l2, SCREEN_W / 2, 142, 2, C_MUTED, C_PANEL, MC_DATUM);
  }

  void stepOnce() {
    snake::StepResult r = game_.step();
    if (r.died || r.won) {
      if (r.won) { fillCell(r.newHead, C_HEAD); }
      gameOver(r.won);
      return;
    }
    if (r.tailRemoved) fillCell(r.oldTail, C_BG);
    fillCell(prevHead_, C_BODY);
    fillCell(r.newHead, C_HEAD);
    prevHead_ = r.newHead;
    if (r.ate) {
      sound::eat();
      drawFood();
      if ((uint32_t)game_.score() > best_) best_ = game_.score();
      drawScore();
    }
  }

  void gameOver(bool filledBoard) {
    state_ = OVER;
    overAt_ = millis();
    bool newBest = (uint32_t)game_.score() > prefGetU32(HS_KEY, 0);
    if (newBest) { prefPutU32(HS_KEY, game_.score()); best_ = game_.score(); }
    drawScore();
    char l2[40];
    snprintf(l2, sizeof(l2), newBest ? "New best: %d!  Tap to retry" : "Score %d  -  tap to retry", game_.score());
    drawOverlay(filledBoard ? "You filled it!" : "Game over", l2);
    if (filledBoard || newBest) { sound::win(); led::flash(false, true, false, 600); }
    else { sound::lose(); led::flash(true, false, false, 500); }
  }
};

}  // namespace

App *makeSnake() {
  static SnakeApp app;
  return &app;
}
