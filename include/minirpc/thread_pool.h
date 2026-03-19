#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace mrpc {

class ThreadPool {
 public:
  ThreadPool(std::size_t thread_count, std::size_t max_queue_size);
  ~ThreadPool();

  bool Submit(std::function<void()> task);
  void Stop();

 private:
  void WorkerLoop();

  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<std::function<void()>> queue_;
  std::vector<std::thread> workers_;
  std::size_t max_queue_size_;
  bool stopping_ = false;
};

}  // namespace mrpc
