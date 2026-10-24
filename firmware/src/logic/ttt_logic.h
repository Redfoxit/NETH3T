#pragma once
// Noughts & crosses (tic-tac-toe) rules and AI. Pure C++, no hardware dependencies.
#include <stdint.h>

namespace ttt {

enum : int8_t { EMPTY = 0, X = 1, O = 2 };
enum Difficulty { EASY = 0, NORMAL = 1, HARD = 2 };

static const uint8_t LINES[8][3] = {
    {0, 1, 2}, {3, 4, 5}, {6, 7, 8},   // rows
    {0, 3, 6}, {1, 4, 7}, {2, 5, 8},   // columns
    {0, 4, 8}, {2, 4, 6}               // diagonals
};

// Returns EMPTY, X or O. If `lineOut` is non-null it receives the winning line index (0-7) or -1.
inline int winner(const int8_t *b, int *lineOut = nullptr) {
  for (int i = 0; i < 8; i++) {
    int8_t a = b[LINES[i][0]];
    if (a != EMPTY && a == b[LINES[i][1]] && a == b[LINES[i][2]]) {
      if (lineOut) *lineOut = i;
      return a;
    }
  }
  if (lineOut) *lineOut = -1;
  return EMPTY;
}

inline bool full(const int8_t *b) {
  for (int i = 0; i < 9; i++) if (b[i] == EMPTY) return false;
  return true;
}

inline int other(int p) { return p == X ? O : X; }

// Score from the point of view of `me`: positive = good for me. Faster wins score higher.
inline int minimax(int8_t *b, int toMove, int me, int depth) {
  int w = winner(b);
  if (w == me) return 10 - depth;
  if (w != EMPTY) return depth - 10;
  if (full(b)) return 0;
  int best = (toMove == me) ? -100 : 100;
  for (int i = 0; i < 9; i++) {
    if (b[i] != EMPTY) continue;
    b[i] = (int8_t)toMove;
    int s = minimax(b, other(toMove), me, depth + 1);
    b[i] = EMPTY;
    if (toMove == me) { if (s > best) best = s; }
    else              { if (s < best) best = s; }
  }
  return best;
}

// Perfect play. Ties are broken using `rnd` so the CYD doesn't always open the same way.
inline int bestMove(int8_t *b, int me, uint32_t rnd) {
  int bestScore = -1000, count = 0, moves[9];
  for (int i = 0; i < 9; i++) {
    if (b[i] != EMPTY) continue;
    b[i] = (int8_t)me;
    int s = minimax(b, other(me), me, 1);
    b[i] = EMPTY;
    if (s > bestScore) { bestScore = s; count = 0; }
    if (s == bestScore) moves[count++] = i;
  }
  return count ? moves[rnd % (uint32_t)count] : -1;
}

inline int randomMove(const int8_t *b, uint32_t rnd) {
  int count = 0, moves[9];
  for (int i = 0; i < 9; i++) if (b[i] == EMPTY) moves[count++] = i;
  return count ? moves[rnd % (uint32_t)count] : -1;
}

// EASY: random. NORMAL: ~60% perfect, otherwise random. HARD: perfect (cannot be beaten).
inline int chooseMove(int8_t *b, int me, Difficulty d, uint32_t rnd) {
  switch (d) {
    case EASY:   return randomMove(b, rnd);
    case NORMAL: return ((rnd >> 8) % 100 < 60) ? bestMove(b, me, rnd) : randomMove(b, rnd);
    default:     return bestMove(b, me, rnd);
  }
}

}  // namespace ttt
