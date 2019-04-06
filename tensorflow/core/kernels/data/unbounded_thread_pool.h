/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#ifndef TENSORFLOW_CORE_KERNELS_DATA_UNBOUNDED_THREAD_POOL_H_
#define TENSORFLOW_CORE_KERNELS_DATA_UNBOUNDED_THREAD_POOL_H_

#include <deque>
#include <memory>
#include <vector>

#include "tensorflow/core/framework/thread_factory.h"
#include "tensorflow/core/lib/core/notification.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/mutex.h"

namespace tensorflow {
namespace data {

// An `UnboundedThreadPool` provides a mechanism for temporally multiplexing a
// potentially large number of "logical" threads onto a smaller number of
// "physical" threads. The multiplexing is achieved by maintaining an internal
// pool of long-running "physical" threads that are used to execute the
// "logical" threads.  Like a regular thread, a "logical" thread may block on
// other threads, and the size of the pool will increase to ensure that progress
// is made. This mechanism is recommended in situations where short-lived
// threads are created repeatedly, to avoid the overhead and memory
// fragmentation that can result from excessive thread creation.
class UnboundedThreadPool {
 public:
  UnboundedThreadPool(Env* env, const string& thread_name)
      : env_(env), thread_name_(thread_name) {}
  ~UnboundedThreadPool();

  // Returns an implementation of `ThreadFactory` that can be used to create
  // logical threads in this pool.
  std::shared_ptr<ThreadFactory> get_thread_factory();

  // Returns the current number of threads in this pool.
  size_t size();

 private:
  class LogicalThreadFactory;
  class LogicalThreadWrapper;
  struct WorkItem {
    std::function<void()> work_function;
    std::shared_ptr<Notification> done_notification;
  };

  std::unique_ptr<Thread> RunOnPooledThread(std::function<void()> fn);
  void PooledThreadFunc();

  Env* const env_;  // Not owned.
  const string thread_name_;
  mutex work_queue_mu_;
  condition_variable work_queue_cv_ GUARDED_BY(work_queue_mu_);
  size_t num_idle_threads_ GUARDED_BY(work_queue_mu_) = 0;
  bool cancelled_ GUARDED_BY(work_queue_mu_) = false;
  std::deque<WorkItem> work_queue_ GUARDED_BY(work_queue_mu_);
  mutex thread_pool_mu_;
  std::vector<std::unique_ptr<Thread>> thread_pool_ GUARDED_BY(thread_pool_mu_);
};

}  // namespace data
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_KERNELS_DATA_UNBOUNDED_THREAD_POOL_H_
