#pragma once
// Wordle rules, daily-word maths and word-table lookups. Pure C++, no hardware dependencies.
// The Python server (wordle-server/server.py) uses identical maths so both agree on the daily word.
#include <stdint.h>
#include <string.h>

namespace wordle {

static const int WORD_LEN = 5;
static const int MAX_GUESSES = 6;

// Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm). No timezone involved.
inline int64_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int64_t yoe = y - era * 400;
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

// Day number for a local calendar date, counted from the Wordle epoch.
inline int64_t dayNumber(int y, int m, int d, int ey, int em, int ed) {
  return daysFromCivil(y, m, d) - daysFromCivil(ey, em, ed);
}

// Maps a day number onto an answer-table index. multiplier must be coprime with count.
inline int answerIndexForDay(int64_t day, int count, int multiplier, int offset) {
  int64_t v = (day % count) * (int64_t)multiplier + offset;
  v %= count;
  if (v < 0) v += count;
  return (int)v;
}

// Per-letter result characters.
static const char HIT = 'g';      // right letter, right place (green)
static const char PRESENT = 'y';  // right letter, wrong place (yellow)
static const char MISS = 'b';     // not in the word (grey)

// Scores `guess` against `answer` (both 5 lowercase letters) into out[0..4] plus a terminator.
// Handles repeated letters the same way the real game does.
inline void scoreGuess(const char *guess, const char *answer, char *out) {
  int remaining[26] = {0};
  for (int i = 0; i < WORD_LEN; i++) out[i] = MISS;
  out[WORD_LEN] = 0;
  for (int i = 0; i < WORD_LEN; i++) {
    if (guess[i] == answer[i]) out[i] = HIT;
    else remaining[answer[i] - 'a']++;
  }
  for (int i = 0; i < WORD_LEN; i++) {
    if (out[i] == HIT) continue;
    int k = guess[i] - 'a';
    if (k >= 0 && k < 26 && remaining[k] > 0) { out[i] = PRESENT; remaining[k]--; }
  }
}

// A table is `count` fixed 5-byte records (no separators), sorted alphabetically.
inline void wordAt(const char *table, int index, char *out) {
  memcpy(out, table + (size_t)index * WORD_LEN, WORD_LEN);
  out[WORD_LEN] = 0;
}

inline bool tableContains(const char *table, int count, const char *word) {
  int lo = 0, hi = count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    int c = memcmp(table + (size_t)mid * WORD_LEN, word, WORD_LEN);
    if (c == 0) return true;
    if (c < 0) lo = mid + 1; else hi = mid - 1;
  }
  return false;
}

// Upgrades a key's colour state: miss < present < hit (never downgrades).
inline char betterState(char current, char candidate) {
  auto rank = [](char c) { return c == HIT ? 3 : c == PRESENT ? 2 : c == MISS ? 1 : 0; };
  return rank(candidate) > rank(current) ? candidate : current;
}

}  // namespace wordle
