#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <queue>

#ifdef ESP8266
namespace std {
struct mutex {
  static void lock() {
    noInterrupts();
  }
  static void unlock() {
    interrupts();
  }
};
}  // namespace std
#endif

class TaskQueue {
  using Runnable = std::function<void()>;

 public:
  TaskQueue() = default;
  TaskQueue(const TaskQueue&) = delete;
  const TaskQueue& operator=(const TaskQueue&) = delete;

  void post(Runnable runnable) {
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.push(std::move(runnable));
  }

  void poll() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!tasks_.empty()) {
      auto runnable = std::move(tasks_.front());
      tasks_.pop();
      lock.unlock();
      runnable();
      lock.lock();
    }
  }

 private:
  std::queue<Runnable> tasks_;
  std::mutex mutex_;
};
