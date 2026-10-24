// Wordle on a 320x240 landscape screen. Both word lists live in flash (words.h), so it works with no
// network at all. If a word server is configured it also supplies the shared daily word and keeps
// streak statistics.
#include <ctype.h>

#include "app.h"
#include "logic/wordle_logic.h"
#include "net.h"
#include "ui.h"
#include "words.h"

using namespace wordle;

namespace {

const int TILE = 23, TGAP = 2, GX = 98, GY = 2;
const int KW = 29, KH = 26, KGAP = 3, KY0 = 154;
const Rect MENU_BTN = {2, 2, 76, 26};
const Rect NEW_BTN = {226, 118, 92, 30};
const Rect INFO_RECT = {226, 4, 92, 20};
const Rect MSG_RECT = {226, 30, 92, 44};
const Rect STAT_RECT = {226, 76, 92, 38};

const uint16_t C_HIT = 0x3E04, C_PRES = 0xCD40, C_MISS = 0x39E7, C_KEY = 0x6B4D, C_KEY_MISS = 0x2104;
const Rect MENU_DAILY = {14, 56, 140, 70};
const Rect MENU_RANDOM = {166, 56, 140, 70};

struct Key {
  Rect r;
  char ch;   // 'a'..'z', '\n' = enter, '\b' = delete
};

class WordleApp : public App {
 public:
  void enter() override {
    done = false;
    layoutKeys();
    state_ = MENU;
    drawMenu();
    touch.consume();
  }

  void tick() override {
    if (state_ == MENU) {
      if (!touch.pressed()) return;
      int px = touch.x(), py = touch.y();
      if (BACK_RECT.hit(px, py)) { done = true; return; }
      if (MENU_DAILY.hit(px, py)) { sound::click(); startGame(true); touch.consume(); }
      else if (MENU_RANDOM.hit(px, py)) { sound::click(); startGame(false); touch.consume(); }
      return;
    }

    if (!touch.pressed()) return;
    int px = touch.x(), py = touch.y();
    if (MENU_BTN.hit(px, py)) { done = true; return; }
    if (NEW_BTN.hit(px, py)) { sound::click(); state_ = MENU; drawMenu(); touch.consume(); return; }
    if (over_) return;
    for (const Key &k : keys_) {
      if (k.r.hit(px, py)) { pressKey(k.ch); break; }
    }
  }

 private:
  enum State { MENU, PLAY };
  State state_ = MENU;
  Key keys_[28];
  char answer_[6] = "";
  char guess_[MAX_GUESSES][6] = {};
  char marks_[MAX_GUESSES][6] = {};
  char keyState_[26] = {};
  int row_ = 0, col_ = 0, dayNum_ = 0;
  bool daily_ = false, over_ = false, won_ = false;

  // ---------- layout ----------
  void layoutKeys() {
    int n = 0;
    const char *r1 = "qwertyuiop", *r2 = "asdfghjkl", *r3 = "zxcvbnm";
    for (int i = 0; i < 10; i++) keys_[n++] = {{(int16_t)(2 + i * (KW + KGAP)), KY0, KW, KH}, r1[i]};
    for (int i = 0; i < 9; i++) keys_[n++] = {{(int16_t)(17 + i * (KW + KGAP)), KY0 + KH + 2, KW, KH}, r2[i]};
    keys_[n++] = {{4, KY0 + 2 * (KH + 2), 42, KH}, '\n'};
    for (int i = 0; i < 7; i++) keys_[n++] = {{(int16_t)(49 + i * (KW + KGAP)), KY0 + 2 * (KH + 2), KW, KH}, r3[i]};
    keys_[n++] = {{273, KY0 + 2 * (KH + 2), 42, KH}, '\b'};
  }

  Rect tileRect(int r, int c) { return Rect{(int16_t)(GX + c * (TILE + TGAP)), (int16_t)(GY + r * (TILE + TGAP)), TILE, TILE}; }

  static uint16_t markColour(char m) { return m == HIT ? C_HIT : m == PRESENT ? C_PRES : C_MISS; }

  // ---------- drawing ----------
  void drawMenu() {
    tft.fillScreen(C_BG);
    drawHeader("Wordle");
    drawButton(MENU_DAILY, "", C_HIT);
    drawText("Daily word", MENU_DAILY.cx(), MENU_DAILY.y + 24, 2, C_TEXT, C_HIT, MC_DATUM);
    drawText("same for everyone", MENU_DAILY.cx(), MENU_DAILY.y + 46, 2, C_TEXT, C_HIT, MC_DATUM);
    drawButton(MENU_RANDOM, "", C_PRES);
    drawText("Random word", MENU_RANDOM.cx(), MENU_RANDOM.y + 24, 2, C_TEXT, C_PRES, MC_DATUM);
    drawText("play as much as you like", MENU_RANDOM.cx(), MENU_RANDOM.y + 46, 1, C_TEXT, C_PRES, MC_DATUM);

    int y, m, d;
    char buf[48];
    uint32_t lastDay = prefGetU32("wd_day", 0xFFFFFFFF);
    uint32_t lastRes = prefGetU32("wd_res", 0);
    if (net::localDate(y, m, d) && lastDay == (uint32_t)dayNumber(y, m, d, WORDLE_EPOCH_Y, WORDLE_EPOCH_M, WORDLE_EPOCH_D)) {
      if (lastRes) snprintf(buf, sizeof(buf), "Today's daily: solved in %u/6", (unsigned)lastRes);
      else snprintf(buf, sizeof(buf), "Today's daily: not solved");
    } else {
      snprintf(buf, sizeof(buf), "Today's daily: not played yet");
    }
    drawText(buf, SCREEN_W / 2, 154, 2, C_TEXT, C_BG, MC_DATUM);
    drawText(net::serverConfigured() ? "Word server: stats + streaks enabled" : "Word server: not set (works offline)",
             SCREEN_W / 2, 180, 2, C_MUTED, C_BG, MC_DATUM);
    if (!net::timeValid()) drawText("Clock not set yet - daily needs WiFi", SCREEN_W / 2, 206, 2, C_WARN, C_BG, MC_DATUM);
  }

  void drawTile(int r, int c) {
    Rect t = tileRect(r, c);
    char letter = guess_[r][c];
    char mark = marks_[r][c];
    if (mark) {
      tft.fillRect(t.x, t.y, t.w, t.h, markColour(mark));
    } else {
      tft.fillRect(t.x, t.y, t.w, t.h, C_BG);
      tft.drawRect(t.x, t.y, t.w, t.h, letter ? C_MUTED : C_PANEL_HI);
    }
    if (letter) {
      char up[2] = {(char)toupper(letter), 0};
      uint16_t bg = mark ? markColour(mark) : C_BG;
      drawText(up, t.cx(), t.cy(), 2, C_TEXT, bg, MC_DATUM);
    }
  }

  void drawKey(const Key &k) {
    uint16_t fill = C_KEY;
    if (k.ch >= 'a' && k.ch <= 'z') {
      char s = keyState_[k.ch - 'a'];
      if (s) fill = (s == HIT) ? C_HIT : (s == PRESENT) ? C_PRES : C_KEY_MISS;
    } else {
      fill = C_PANEL_HI;
    }
    const char *label;
    char one[2] = {(char)toupper(k.ch), 0};
    if (k.ch == '\n') label = "ENT";
    else if (k.ch == '\b') label = "DEL";
    else label = one;
    drawButton(k.r, label, fill, C_TEXT, 2);
  }

  void drawKeyboard() { for (const Key &k : keys_) drawKey(k); }

  void setMsg(const char *text, uint16_t colour = C_TEXT) { drawLines(MSG_RECT, text, 2, colour, C_BG, 20); }

  void drawGame() {
    tft.fillScreen(C_BG);
    drawButton(MENU_BTN, "< Menu", C_PANEL_HI);
    for (int r = 0; r < MAX_GUESSES; r++)
      for (int c = 0; c < WORD_LEN; c++) drawTile(r, c);
    drawKeyboard();
    drawButton(NEW_BTN, "New game", C_PANEL_HI);
    char info[24];
    if (daily_) snprintf(info, sizeof(info), "Daily #%d", dayNum_);
    else snprintf(info, sizeof(info), "Random");
    drawLines(INFO_RECT, info, 2, C_ACCENT, C_BG, 20);
  }

  // ---------- game flow ----------
  void startGame(bool daily) {
    int idx = -1;
    dayNum_ = 0;
    daily_ = daily;
    const char *notice = nullptr;
    if (daily) {
      centerMessage("Getting today's word...");
      net::DailyInfo di = net::fetchDaily();
      int y, m, d;
      if (di.ok && di.answers == WORDS_ANSWER_COUNT && di.index < WORDS_ANSWER_COUNT) {
        idx = di.index;
        dayNum_ = di.day;
      } else if (net::localDate(y, m, d)) {
        int64_t day = dayNumber(y, m, d, WORDLE_EPOCH_Y, WORDLE_EPOCH_M, WORDLE_EPOCH_D);
        idx = answerIndexForDay(day, WORDS_ANSWER_COUNT, WORDS_DAY_MULTIPLIER, WORDS_DAY_OFFSET);
        dayNum_ = (int)day;
      } else {
        daily_ = false;
        notice = "No clock yet:\nrandom word";
      }
    }
    if (idx < 0) idx = (int)(esp_random() % WORDS_ANSWER_COUNT);
    wordAt(WORDS_ANSWERS, idx, answer_);

    memset(guess_, 0, sizeof(guess_));
    memset(marks_, 0, sizeof(marks_));
    memset(keyState_, 0, sizeof(keyState_));
    row_ = col_ = 0;
    over_ = won_ = false;
    state_ = PLAY;
    drawGame();
    setMsg(notice ? notice : "Guess the\n5-letter word", notice ? C_WARN : C_MUTED);
  }

  void pressKey(char ch) {
    if (ch == '\b') {
      if (col_ > 0) { guess_[row_][--col_] = 0; drawTile(row_, col_); sound::click(); }
      return;
    }
    if (ch == '\n') { submit(); return; }
    if (col_ < WORD_LEN) {
      guess_[row_][col_] = ch;
      guess_[row_][col_ + 1] = 0;
      drawTile(row_, col_);
      col_++;
      sound::click();
    }
  }

  void submit() {
    if (col_ < WORD_LEN) { setMsg("Need 5\nletters", C_WARN); sound::bad(); return; }
    if (!tableContains(WORDS_GUESSES, WORDS_GUESS_COUNT, guess_[row_])) {
      setMsg("Not in\nword list", C_WARN);
      sound::bad();
      return;
    }
    scoreGuess(guess_[row_], answer_, marks_[row_]);
    for (int c = 0; c < WORD_LEN; c++) {   // little reveal animation
      drawTile(row_, c);
      sound::click();
      delay(80);
    }
    for (int c = 0; c < WORD_LEN; c++) {
      int k = guess_[row_][c] - 'a';
      keyState_[k] = betterState(keyState_[k], marks_[row_][c]);
    }
    drawKeyboard();

    bool allHit = true;
    for (int c = 0; c < WORD_LEN; c++) if (marks_[row_][c] != HIT) allHit = false;
    int used = row_ + 1;
    if (allHit) {
      won_ = true;
      over_ = true;
      char msg[32];
      snprintf(msg, sizeof(msg), "Got it in\n%d of 6!", used);
      setMsg(msg, C_GOOD);
      sound::win();
      led::flash(false, true, false, 700);
      finish(used);
    } else if (used >= MAX_GUESSES) {
      over_ = true;
      char up[8];
      for (int i = 0; i < WORD_LEN; i++) up[i] = (char)toupper(answer_[i]);
      up[WORD_LEN] = 0;
      char msg[32];
      snprintf(msg, sizeof(msg), "The word was\n%s", up);
      setMsg(msg, C_BAD);
      sound::lose();
      led::flash(true, false, false, 700);
      finish(used);
    } else {
      row_++;
      col_ = 0;
      setMsg("");
    }
  }

  void finish(int used) {
    if (daily_) {
      prefPutU32("wd_day", (uint32_t)dayNum_);
      prefPutU32("wd_res", won_ ? (uint32_t)used : 0);
    }
    if (!net::serverConfigured()) return;
    net::Stats s = net::reportResult(daily_, dayNum_, answer_, won_, used);
    if (s.ok) {
      char buf[48];
      snprintf(buf, sizeof(buf), "Streak %d\nWon %d / %d", s.streak, s.wins, s.played);
      drawLines(STAT_RECT, buf, 2, C_MUTED, C_BG, 18);
    } else {
      drawLines(STAT_RECT, "Server\noffline", 2, C_MUTED, C_BG, 18);
    }
  }
};

}  // namespace

App *makeWordle() {
  static WordleApp app;
  return &app;
}
