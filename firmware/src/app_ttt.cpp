// Noughts & crosses: 1 player vs CYD (easy / normal / unbeatable) or 2 players on one screen.
#include "app.h"
#include "logic/ttt_logic.h"
#include "ui.h"

namespace {

const int CELL = 68, BX = 10, BY = 30;
const Rect MODE_BTN = {226, 150, 88, 36};
const Rect NEXT_BTN = {226, 192, 88, 42};
const Rect PANEL = {222, 28, 98, 120};

const Rect SELECT_BTN[4] = {{14, 56, 140, 56}, {166, 56, 140, 56}, {14, 128, 140, 56}, {166, 128, 140, 56}};
const char *SELECT_L1[4] = {"vs CYD", "vs CYD", "vs CYD", "2 Players"};
const char *SELECT_L2[4] = {"Easy", "Normal", "Unbeatable", "same screen"};
const uint16_t SELECT_COL[4] = {0x2C6B, 0xB5A0, 0xA8A2, 0x3A9C};

class TicTacToe : public App {
 public:
  void enter() override {
    done = false;
    state_ = SELECT;
    drawSelect();
    touch.consume();
  }

  void tick() override {
    if (backTapped()) { done = true; return; }

    if (state_ == SELECT) {
      if (!touch.pressed()) return;
      for (int i = 0; i < 4; i++) {
        if (!SELECT_BTN[i].hit(touch.x(), touch.y())) continue;
        sound::click();
        vsAi_ = i < 3;
        diff_ = (i == 0) ? ttt::EASY : (i == 1) ? ttt::NORMAL : ttt::HARD;
        scoreX_ = scoreO_ = draws_ = 0;
        round_ = 0;
        startRound();
        touch.consume();
        return;
      }
      return;
    }

    if (touch.pressed()) {
      int px = touch.x(), py = touch.y();
      if (MODE_BTN.hit(px, py)) { sound::click(); state_ = SELECT; drawSelect(); touch.consume(); return; }
      if (NEXT_BTN.hit(px, py)) { sound::click(); startRound(); touch.consume(); return; }
      bool humanTurn = !(vsAi_ && turn_ == ttt::O);
      if (state_ == PLAYING && humanTurn && px >= BX && py >= BY && px < BX + 3 * CELL && py < BY + 3 * CELL) {
        int cell = ((py - BY) / CELL) * 3 + (px - BX) / CELL;
        if (board_[cell] == ttt::EMPTY) place(cell);
      }
    }

    if (state_ == PLAYING && vsAi_ && turn_ == ttt::O && millis() >= aiAt_) {
      int mv = ttt::chooseMove(board_, ttt::O, diff_, esp_random());
      if (mv >= 0) place(mv);
    }
  }

 private:
  enum State { SELECT, PLAYING, OVER };
  State state_ = SELECT;
  bool vsAi_ = true;
  ttt::Difficulty diff_ = ttt::HARD;
  int8_t board_[9] = {0};
  int turn_ = ttt::X, round_ = 0, winLine_ = -1;
  int scoreX_ = 0, scoreO_ = 0, draws_ = 0;
  uint32_t aiAt_ = 0;

  void drawSelect() {
    tft.fillScreen(C_BG);
    drawHeader("Noughts & Crosses");
    drawText("Choose a mode", SCREEN_W / 2, 40, 2, C_MUTED, C_BG, MC_DATUM);
    for (int i = 0; i < 4; i++) {
      const Rect &r = SELECT_BTN[i];
      tft.fillRoundRect(r.x, r.y, r.w, r.h, 8, SELECT_COL[i]);
      drawText(SELECT_L1[i], r.cx(), r.y + 18, 2, C_TEXT, SELECT_COL[i], MC_DATUM);
      drawText(SELECT_L2[i], r.cx(), r.y + 38, 2, C_TEXT, SELECT_COL[i], MC_DATUM);
    }
  }

  void startRound() {
    memset(board_, 0, sizeof(board_));
    winLine_ = -1;
    turn_ = (round_ % 2 == 0) ? ttt::X : ttt::O;   // alternate who starts
    round_++;
    state_ = PLAYING;
    tft.fillScreen(C_BG);
    drawHeader(vsAi_ ? (diff_ == ttt::EASY ? "vs CYD - Easy" : diff_ == ttt::NORMAL ? "vs CYD - Normal" : "vs CYD - Unbeatable")
                     : "Two players");
    drawBoard();
    drawPanel();
    drawButton(MODE_BTN, "Mode", C_PANEL_HI);
    drawButton(NEXT_BTN, "Next round", C_PANEL_HI);
    if (vsAi_ && turn_ == ttt::O) aiAt_ = millis() + 600;
  }

  void cellCentre(int i, int &cx, int &cy) {
    cx = BX + (i % 3) * CELL + CELL / 2;
    cy = BY + (i / 3) * CELL + CELL / 2;
  }

  void drawMark(int i, int who) {
    int cx, cy;
    cellCentre(i, cx, cy);
    if (who == ttt::X) {
      for (int t = -2; t <= 2; t++) {
        tft.drawLine(cx - 20 + t, cy - 20, cx + 20 + t, cy + 20, C_X);
        tft.drawLine(cx + 20 + t, cy - 20, cx - 20 + t, cy + 20, C_X);
      }
    } else if (who == ttt::O) {
      tft.fillCircle(cx, cy, 23, C_O);
      tft.fillCircle(cx, cy, 16, C_BG);
    }
  }

  void drawBoard() {
    tft.fillRect(BX - 2, BY - 2, 3 * CELL + 4, 3 * CELL + 4, C_BG);
    for (int i = 1; i < 3; i++) {
      tft.fillRect(BX + i * CELL - 1, BY + 4, 3, 3 * CELL - 8, C_PANEL_HI);
      tft.fillRect(BX + 4, BY + i * CELL - 1, 3 * CELL - 8, 3, C_PANEL_HI);
    }
    for (int i = 0; i < 9; i++) drawMark(i, board_[i]);
  }

  void drawWinLine() {
    if (winLine_ < 0) return;
    int ax, ay, bx, by;
    cellCentre(ttt::LINES[winLine_][0], ax, ay);
    cellCentre(ttt::LINES[winLine_][2], bx, by);
    for (int dx = -1; dx <= 1; dx++)
      for (int dy = -1; dy <= 1; dy++) tft.drawLine(ax + dx, ay + dy, bx + dx, by + dy, C_GOOD);
  }

  void drawPanel() {
    tft.fillRect(PANEL.x, PANEL.y, PANEL.w, PANEL.h, C_BG);
    char buf[40];
    const char *status;
    if (state_ == PLAYING) {
      status = vsAi_ ? (turn_ == ttt::X ? "Your turn" : "CYD thinks") : (turn_ == ttt::X ? "X to play" : "O to play");
    } else if (winLine_ >= 0) {
      int w = board_[ttt::LINES[winLine_][0]];
      status = vsAi_ ? (w == ttt::X ? "You win!" : "CYD wins") : (w == ttt::X ? "X wins!" : "O wins!");
    } else {
      status = "Draw";
    }
    drawText(status, PANEL.x + 2, PANEL.y + 2, 2, C_TEXT, C_BG);
    const char *lx = vsAi_ ? "You" : "X";
    const char *lo = vsAi_ ? "CYD" : "O";
    snprintf(buf, sizeof(buf), "%s  %d", lx, scoreX_);
    drawText(buf, PANEL.x + 2, PANEL.y + 40, 2, C_X, C_BG);
    snprintf(buf, sizeof(buf), "%s  %d", lo, scoreO_);
    drawText(buf, PANEL.x + 2, PANEL.y + 62, 2, C_O, C_BG);
    snprintf(buf, sizeof(buf), "Draws  %d", draws_);
    drawText(buf, PANEL.x + 2, PANEL.y + 84, 2, C_MUTED, C_BG);
  }

  void place(int cell) {
    board_[cell] = (int8_t)turn_;
    drawMark(cell, turn_);
    sound::click();
    int line = -1;
    int w = ttt::winner(board_, &line);
    if (w != ttt::EMPTY) {
      winLine_ = line;
      state_ = OVER;
      if (w == ttt::X) scoreX_++; else scoreO_++;
      drawWinLine();
      if (vsAi_ && w == ttt::O) { sound::lose(); led::flash(true, false, false, 600); }
      else { sound::win(); led::flash(false, true, false, 600); }
      drawPanel();
    } else if (ttt::full(board_)) {
      state_ = OVER;
      draws_++;
      sound::good();
      drawPanel();
    } else {
      turn_ = ttt::other(turn_);
      drawPanel();
      if (vsAi_ && turn_ == ttt::O) aiAt_ = millis() + 450;
    }
  }
};

}  // namespace

App *makeTicTacToe() {
  static TicTacToe app;
  return &app;
}
