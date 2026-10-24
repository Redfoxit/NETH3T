#pragma once
// Snake rules. Pure C++, no hardware dependencies.
#include <stdint.h>
#include <string.h>

namespace snake {

enum Dir : uint8_t { UP = 0, RIGHT = 1, DOWN = 2, LEFT = 3 };

struct Pt {
  int8_t x, y;
};

struct StepResult {
  bool died = false;
  bool ate = false;
  bool won = false;        // filled the whole board
  Pt newHead{0, 0};
  bool tailRemoved = false;  // true when the tail cell was vacated (didn't eat)
  Pt oldTail{0, 0};
};

template <int W, int H>
class Game {
 public:
  static const int MAXLEN = W * H;

  void reset(uint32_t seed, bool wrapWalls = false) {
    rng_ = seed ? seed : 0x9E3779B9u;
    wrap_ = wrapWalls;
    len_ = 3;
    int cx = W / 2, cy = H / 2;
    for (int i = 0; i < len_; i++) body_[i] = Pt{(int8_t)(cx - i), (int8_t)cy};  // head first
    dir_ = RIGHT;
    qLen_ = 0;
    score_ = 0;
    alive_ = true;
    placeFood();
  }

  // Queue a direction change (up to 2 buffered). Ignores reversals and duplicates.
  bool turn(Dir d) {
    Dir last = qLen_ ? queue_[qLen_ - 1] : dir_;
    if (d == last || (uint8_t)((d + 2) & 3) == last) return false;
    if (qLen_ >= 2) return false;
    queue_[qLen_++] = d;
    return true;
  }

  StepResult step() {
    StepResult r;
    if (!alive_) { r.died = true; return r; }
    if (qLen_) {
      dir_ = queue_[0];
      queue_[0] = queue_[1];
      qLen_--;
    }
    int nx = body_[0].x, ny = body_[0].y;
    switch (dir_) {
      case UP: ny--; break;
      case DOWN: ny++; break;
      case LEFT: nx--; break;
      default: nx++; break;
    }
    if (wrap_) {
      nx = (nx + W) % W;
      ny = (ny + H) % H;
    } else if (nx < 0 || ny < 0 || nx >= W || ny >= H) {
      alive_ = false; r.died = true; return r;
    }
    bool eating = (nx == food_.x && ny == food_.y);
    // The tail cell is vacated this step unless we are growing, so moving into it is legal.
    int checkLen = eating ? len_ : len_ - 1;
    for (int i = 0; i < checkLen; i++) {
      if (body_[i].x == nx && body_[i].y == ny) { alive_ = false; r.died = true; return r; }
    }
    Pt tail = body_[len_ - 1];
    if (eating) {
      memmove(&body_[1], &body_[0], sizeof(Pt) * len_);
      len_++;
      score_++;
    } else {
      memmove(&body_[1], &body_[0], sizeof(Pt) * (len_ - 1));
      r.tailRemoved = true;
      r.oldTail = tail;
    }
    body_[0] = Pt{(int8_t)nx, (int8_t)ny};
    r.newHead = body_[0];
    r.ate = eating;
    if (eating) {
      if (len_ >= MAXLEN) { r.won = true; alive_ = false; }
      else placeFood();
    }
    return r;
  }

  bool alive() const { return alive_; }
  int score() const { return score_; }
  int length() const { return len_; }
  Pt food() const { return food_; }
  Pt segment(int i) const { return body_[i]; }
  Dir direction() const { return dir_; }

 private:
  uint32_t next() {  // xorshift32
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return rng_;
  }
  void placeFood() {
    int free = MAXLEN - len_;
    if (free <= 0) return;
    int pick = (int)(next() % (uint32_t)free);
    for (int y = 0; y < H; y++) {
      for (int x = 0; x < W; x++) {
        bool occupied = false;
        for (int i = 0; i < len_; i++) if (body_[i].x == x && body_[i].y == y) { occupied = true; break; }
        if (occupied) continue;
        if (pick-- == 0) { food_ = Pt{(int8_t)x, (int8_t)y}; return; }
      }
    }
  }

  Pt body_[MAXLEN];
  Pt food_{0, 0};
  Dir queue_[2] = {RIGHT, RIGHT};
  Dir dir_ = RIGHT;
  int qLen_ = 0, len_ = 0, score_ = 0;
  bool alive_ = false, wrap_ = false;
  uint32_t rng_ = 1;
};

}  // namespace snake
