// Copyright (c) 2016, Tamas Csala

#include "timer.hpp"

namespace engine {

double Timer::Tick() {
  if (!stopped_) {
    double time = glfwGetTime();
    if (last_time_ != 0) {
      dt_ = time - last_time_;
    }
    last_time_ = time;
    current_time_ += dt_;
  }
  return current_time_;
}

void Timer::Stop() {
  stopped_ = true;
  dt_ = 0;
}

void Timer::Start() {
  stopped_ = false;
  last_time_ = glfwGetTime();
}

void Timer::Toggle() {
  if (stopped_) {
    Start();
  } else {
    Stop();
  }
}

}  // namespace engine
