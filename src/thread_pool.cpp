#include "minirpc/thread_pool.h"

namespace mrpc {

ThreadPool::ThreadPool(std::size_t thread_count, std::size_t max_queue_size)
    : max_queue_size_(max_queue_size == 0 ? 1 : max_queue_size) {
  // 保证线程池至少有一个工作线程，避免配置为 0 时完全失去执行能力。
  const auto workers = thread_count == 0 ? 1 : thread_count;
  workers_.reserve(workers);
  for (std::size_t i = 0; i < workers; ++i) {
    workers_.emplace_back([this]() { WorkerLoop(); });
  }
}

ThreadPool::~ThreadPool() {
  Stop();
}

bool ThreadPool::Submit(std::function<void()> task) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (stopping_ || queue_.size() >= max_queue_size_) {
    return false;
  }
  queue_.push_back(std::move(task));
  cv_.notify_one();
  return true;
}

void ThreadPool::Stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) {
      return;
    }
    stopping_ = true;
  }
  cv_.notify_all();
  for (auto& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  workers_.clear();
}

void ThreadPool::WorkerLoop() {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this]() { return stopping_ || !queue_.empty(); });
      // Stop 之后继续把队列清空，确保已接收的 RPC 不会半途丢失。
      if (stopping_ && queue_.empty()) {
        return;
      }
      task = std::move(queue_.front());
      queue_.pop_front();
    }
    task();
  }
}

}  // namespace mrpc
