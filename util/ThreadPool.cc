#include <cassert>
#include "ThreadPool.h"

namespace ananas {
std::thread::id ThreadPool::s_mainThread;

ThreadPool::ThreadPool() {
    // init main thread id
    s_mainThread = std::this_thread::get_id();
}

ThreadPool::~ThreadPool() {
    JoinAll();
}

void ThreadPool::SetNumOfThreads(int n) {
    assert(n >= 0 && n <= kMaxThreads);
    numThreads_ = n;
}

void ThreadPool::_Start() {
  if (shutdown_) {
    return;
  }

  assert(workers_.empty());

  for (int i = 0; i < numThreads_; i++) {
    std::thread t([this]() { this->_WorkerRoutine(); });    // 线程执行this->_WorkerRoutine()函数
    workers_.push_back(std::move(t));   // 创建线程并将线程放入到workers_中
  }
}

void ThreadPool::JoinAll() {
    if (s_mainThread != std::this_thread::get_id())
        return;

    decltype(workers_)  tmp;

    {
        std::unique_lock<std::mutex>  guard(mutex_);
        if (shutdown_)
            return;

        shutdown_ = true;
        cond_.notify_all();

        tmp.swap(workers_);
    }

    for (auto& t : tmp) {
        if (t.joinable())
            t.join();
    }
}

// 子线程创建初始化执行的函数, 从task列表去除task来执行
void ThreadPool::_WorkerRoutine() {
    while (true) {
        std::function<void ()> task;
        // 取出task, 这是个阻塞队列
        {
            std::unique_lock<std::mutex> guard(mutex_);

            cond_.wait(guard, [this]() {
                return shutdown_ || !tasks_.empty();
            } );

            assert(shutdown_ || !tasks_.empty());
            if (shutdown_ && tasks_.empty()) {
                return;
            }

            assert (!tasks_.empty());
            task = std::move(tasks_.front());
            tasks_.pop_front();
        }

        task();
    }
}

size_t ThreadPool::WorkerThreads() const {
  std::unique_lock<std::mutex> guard(mutex_);
  return workers_.size();
}

size_t ThreadPool::Tasks() const {
  std::unique_lock<std::mutex> guard(mutex_);
  return tasks_.size();
}

} // namespace ananas

