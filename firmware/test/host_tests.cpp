// Native (laptop) tests for the pure game logic. No ESP32 needed.
//   g++ -std=c++17 -Wall -Wextra -O1 -I../src test/host_tests.cpp -o /tmp/host_tests && /tmp/host_tests
// With an argument "dates" it prints daily-word indexes for cross-checking against the Python server.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../src/logic/snake_logic.h"
#include "../src/logic/ttt_logic.h"
#include "../src/logic/wordle_logic.h"
#include "../src/words.h"

static int failures = 0, checks = 0;
#define CHECK(cond, msg)                                              \
  do {                                                                \
    checks++;                                                         \
    if (!(cond)) { failures++; printf("FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); } \
  } while (0)

static uint32_t rngState = 12345;
static uint32_t rnd() { rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5; return rngState; }

// ---------- noughts & crosses ----------
static int playGame(ttt::Difficulty xDiff, bool xIsAi, ttt::Difficulty oDiff, bool oIsAi, bool xFirst) {
  int8_t b[9] = {0};
  int turn = xFirst ? ttt::X : ttt::O;
  while (!ttt::winner(b) && !ttt::full(b)) {
    int mv;
    bool ai = (turn == ttt::X) ? xIsAi : oIsAi;
    ttt::Difficulty d = (turn == ttt::X) ? xDiff : oDiff;
    mv = ai ? ttt::chooseMove(b, turn, d, rnd()) : ttt::randomMove(b, rnd());
    if (mv < 0 || b[mv] != ttt::EMPTY) { printf("illegal move chosen\n"); failures++; return -1; }
    b[mv] = (int8_t)turn;
    turn = ttt::other(turn);
  }
  return ttt::winner(b);
}

static void testTicTacToe() {
  int8_t b[9] = {ttt::X, ttt::X, ttt::X, 0, 0, 0, 0, 0, 0};
  int line = -1;
  CHECK(ttt::winner(b, &line) == ttt::X && line == 0, "row win detected");
  int8_t d[9] = {ttt::O, 0, 0, 0, ttt::O, 0, 0, 0, ttt::O};
  CHECK(ttt::winner(d, &line) == ttt::O && line == 6, "diagonal win detected");
  int8_t e[9] = {ttt::X, ttt::O, ttt::X, ttt::X, ttt::O, ttt::O, ttt::O, ttt::X, ttt::X};
  CHECK(ttt::winner(e) == ttt::EMPTY && ttt::full(e), "draw board");

  // AI takes an immediate win and blocks an immediate loss.
  int8_t w[9] = {ttt::O, ttt::O, 0, ttt::X, ttt::X, 0, 0, 0, 0};
  CHECK(ttt::bestMove(w, ttt::O, 1) == 2, "AI takes the win");
  int8_t k[9] = {ttt::X, ttt::X, 0, 0, ttt::O, 0, 0, 0, 0};
  CHECK(ttt::bestMove(k, ttt::O, 1) == 2, "AI blocks");

  // Perfect vs perfect always draws; perfect never loses to random play, either seat.
  for (int i = 0; i < 40; i++) {
    CHECK(playGame(ttt::HARD, true, ttt::HARD, true, i & 1) == ttt::EMPTY, "hard vs hard draws");
  }
  int losses = 0, wins = 0;
  const int N = 400;
  for (int i = 0; i < N; i++) {
    int r = playGame(ttt::HARD, true, ttt::HARD, false, i & 1);  // AI = X, random = O
    if (r == ttt::O) losses++;
    if (r == ttt::X) wins++;
    r = playGame(ttt::HARD, false, ttt::HARD, true, i & 1);      // AI = O, random = X
    if (r == ttt::X) losses++;
    if (r == ttt::O) wins++;
  }
  CHECK(losses == 0, "HARD AI never loses to random play");
  printf("  ttt: hard AI won %d of %d games vs random, lost %d\n", wins, 2 * N, losses);

  // EASY should be beatable: random vs EASY should lose sometimes.
  int easyLosses = 0;
  for (int i = 0; i < N; i++) if (playGame(ttt::EASY, true, ttt::HARD, true, i & 1) == ttt::O) easyLosses++;
  CHECK(easyLosses > 0, "EASY AI can lose to perfect play");
}

// ---------- snake ----------
static void testSnake() {
  snake::Game<8, 6> g;
  g.reset(42);
  CHECK(g.length() == 3 && g.alive() && g.score() == 0, "snake initial state");
  CHECK(!g.turn(snake::LEFT), "reversal into own neck is ignored");
  CHECK(!g.turn(snake::RIGHT), "same direction ignored");

  // Walk right until the wall: must die, regardless of food placement.
  snake::Game<8, 6> w;
  w.reset(7);
  bool died = false;
  for (int i = 0; i < 20 && !died; i++) died = w.step().died;
  CHECK(died && !w.alive(), "hits wall and dies");

  // Wrap mode never dies running in a straight line on an empty-ish board before eating.
  snake::Game<8, 6> wr;
  wr.reset(7, true);
  bool diedWrap = false;
  for (int i = 0; i < 8 && !diedWrap; i++) diedWrap = wr.step().died;
  CHECK(!diedWrap, "wrap mode passes through walls");

  // Eating grows the snake and scores. Steer towards food using a greedy walker.
  snake::Game<8, 6> f;
  f.reset(99, true);
  int ate = 0;
  for (int i = 0; i < 4000 && f.alive() && ate < 10; i++) {
    snake::Pt h = f.segment(0), fd = f.food();
    snake::Dir want = snake::RIGHT;
    if (h.x != fd.x) want = (fd.x > h.x) ? snake::RIGHT : snake::LEFT;
    else want = (fd.y > h.y) ? snake::DOWN : snake::UP;
    f.turn(want);
    snake::StepResult r = f.step();
    if (r.ate) ate++;
  }
  CHECK(ate >= 3, "greedy walker eats several foods");
  CHECK(f.length() == 3 + f.score(), "length tracks score");

  // Body occupancy invariant: no duplicate segments and food never inside the snake.
  bool dup = false, foodInside = false;
  for (int i = 0; i < f.length(); i++) {
    for (int j = i + 1; j < f.length(); j++)
      if (f.segment(i).x == f.segment(j).x && f.segment(i).y == f.segment(j).y) dup = true;
    if (f.segment(i).x == f.food().x && f.segment(i).y == f.food().y) foodInside = true;
  }
  CHECK(!dup && !foodInside, "no overlapping segments, food outside snake");

  // Self collision: a snake that curls into itself dies.
  snake::Game<8, 6> s;
  s.reset(5, true);
  // grow artificially by running greedy until length >= 5, then spiral
  for (int i = 0; i < 4000 && s.length() < 6 && s.alive(); i++) {
    snake::Pt h = s.segment(0), fd = s.food();
    snake::Dir want = (h.x != fd.x) ? ((fd.x > h.x) ? snake::RIGHT : snake::LEFT) : ((fd.y > h.y) ? snake::DOWN : snake::UP);
    s.turn(want);
    s.step();
  }
  bool selfDied = false;
  if (s.alive()) {
    snake::Dir seq[] = {snake::DOWN, snake::LEFT, snake::UP, snake::RIGHT, snake::DOWN, snake::LEFT, snake::UP};
    for (auto d : seq) { s.turn(d); if (s.step().died) { selfDied = true; break; } }
    for (int i = 0; i < 6 && !selfDied; i++) { s.turn(snake::UP); if (s.step().died) selfDied = true; }
  }
  CHECK(selfDied || !s.alive(), "curling into itself eventually dies (no ghost body)");
}

// ---------- wordle ----------
static std::string score(const char *g, const char *a) {
  char out[8];
  wordle::scoreGuess(g, a, out);
  return out;
}

static void testWordle() {
  CHECK(score("crane", "crane") == "ggggg", "exact match");
  CHECK(score("speed", "abide") == "bbybY" || score("speed", "abide") == "bbyby", "speed vs abide");
  CHECK(score("speed", "abide") == "bbyby", "speed vs abide (duplicate e only scores once)");
  CHECK(score("geese", "erase") == "bybgg", "geese vs erase");
  CHECK(score("llama", "hello") == "yybbb", "llama vs hello (two l's in answer, both present)");
  CHECK(score("hello", "llama") == "bbyyb", "hello vs llama");
  CHECK(score("lolly", "hello") == "byggb", "lolly vs hello (greens consume both l's, first l misses)");
  CHECK(score("aaaaa", "banana") != "", "no crash on repeated letters");
  CHECK(score("level", "bevel") == "bgggg", "level vs bevel");

  CHECK(wordle::daysFromCivil(1970, 1, 1) == 0, "epoch is day 0");
  CHECK(wordle::daysFromCivil(2000, 3, 1) == 11017, "2000-03-01");
  CHECK(wordle::dayNumber(2021, 6, 19, 2021, 6, 19) == 0, "wordle epoch day 0");
  CHECK(wordle::dayNumber(2021, 6, 20, 2021, 6, 19) == 1, "next day is 1");
  CHECK(wordle::dayNumber(2024, 3, 1, 2024, 2, 28) == 2, "leap year handled");

  // Daily index is a permutation: over one full cycle every answer appears exactly once.
  std::vector<int> seen(WORDS_ANSWER_COUNT, 0);
  for (int d = 0; d < WORDS_ANSWER_COUNT; d++)
    seen[wordle::answerIndexForDay(d, WORDS_ANSWER_COUNT, WORDS_DAY_MULTIPLIER, WORDS_DAY_OFFSET)]++;
  bool perm = true;
  for (int c : seen) if (c != 1) perm = false;
  CHECK(perm, "daily mapping hits every answer exactly once per cycle");

  // Tables.
  CHECK(sizeof(WORDS_ANSWERS) - 1 == (size_t)WORDS_ANSWER_COUNT * 5, "answers table size");
  CHECK(sizeof(WORDS_GUESSES) - 1 == (size_t)WORDS_GUESS_COUNT * 5, "guesses table size");
  CHECK(wordle::tableContains(WORDS_GUESSES, WORDS_GUESS_COUNT, "crane"), "crane is a valid guess");
  CHECK(!wordle::tableContains(WORDS_GUESSES, WORDS_GUESS_COUNT, "zzzzq"), "nonsense rejected");
  bool allAnswersValid = true, sorted = true;
  for (int i = 0; i < WORDS_ANSWER_COUNT; i++) {
    char w[6], p[6];
    wordle::wordAt(WORDS_ANSWERS, i, w);
    if (!wordle::tableContains(WORDS_GUESSES, WORDS_GUESS_COUNT, w)) allAnswersValid = false;
    if (i) { wordle::wordAt(WORDS_ANSWERS, i - 1, p); if (strcmp(p, w) >= 0) sorted = false; }
  }
  CHECK(allAnswersValid, "every answer is also a valid guess");
  CHECK(sorted, "answers sorted and unique");
  bool guessSorted = true;
  for (int i = 1; i < WORDS_GUESS_COUNT; i++)
    if (memcmp(WORDS_GUESSES + (size_t)(i - 1) * 5, WORDS_GUESSES + (size_t)i * 5, 5) >= 0) guessSorted = false;
  CHECK(guessSorted, "guess table sorted (binary search is valid)");
  bool lower = true;
  for (size_t i = 0; i < sizeof(WORDS_GUESSES) - 1; i++) if (WORDS_GUESSES[i] < 'a' || WORDS_GUESSES[i] > 'z') lower = false;
  CHECK(lower, "guess table is lowercase letters only");
}

int main(int argc, char **argv) {
  if (argc > 1 && !strcmp(argv[1], "dates")) {
    // y m d -> day number and answer index, for comparison with the Python server.
    int dates[][3] = {{2021, 6, 19}, {2021, 6, 20}, {2024, 2, 29}, {2026, 10, 24}, {2027, 1, 1}, {2030, 12, 31}};
    for (auto &dt : dates) {
      int64_t day = wordle::dayNumber(dt[0], dt[1], dt[2], 2021, 6, 19);
      int idx = wordle::answerIndexForDay(day, WORDS_ANSWER_COUNT, WORDS_DAY_MULTIPLIER, WORDS_DAY_OFFSET);
      char w[6];
      wordle::wordAt(WORDS_ANSWERS, idx, w);
      printf("%04d-%02d-%02d %lld %d %s\n", dt[0], dt[1], dt[2], (long long)day, idx, w);
    }
    return 0;
  }
  testTicTacToe();
  testSnake();
  testWordle();
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
