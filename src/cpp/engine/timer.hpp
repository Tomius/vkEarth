// Copyright (c) 2016, Tamas Csala

#ifndef ENGINE_TIMER_H_
#define ENGINE_TIMER_H_

#include <GLFW/glfw3.h>

namespace engine {

class Timer {
  double last_time_, current_time_, dt_;
  bool stopped_;

 public:
  Timer() : last_time_(0), current_time_(0), dt_(0), stopped_(false) {}

  double Tick();
  void   Stop();
  void   Start();
  void   Toggle();

  double current_time() const { return current_time_; }
  double dt() const { return dt_; }
};

}  // namespace engine

#endif
