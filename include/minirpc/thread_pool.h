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

  // 有界队列提交；返回 false 表示线程池已停止或排队任务过多，调用方可选择快速失败。
  bool Submit(std::function<void()> task);
  void Stop();

 private:
  void WorkerLoop();

  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<std::function<void()>> queue_;
  std::vector<std::thread> workers_;
  std::size_t max_queue_size_;
  // Stop 之后不再接受新任务，但会把队列里已接收的任务处理完。
  bool stopping_ = false;
};

}  // namespace mrpc
