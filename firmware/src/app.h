#pragma once
// Every screen/game is an App: enter() draws it, tick() runs each loop, set `done` to return to the menu.
class App {
 public:
  virtual ~App() {}
  virtual void enter() = 0;
  virtual void tick() = 0;
  bool done = false;
};

App *makeTicTacToe();
App *makeSnake();
App *makeWordle();
App *makeSettings();
