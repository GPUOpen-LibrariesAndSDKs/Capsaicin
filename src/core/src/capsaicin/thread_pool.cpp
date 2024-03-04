/**********************************************************************
Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/
#include "thread_pool.h"

namespace Capsaicin
{
bool                  ThreadPool::terminate_;
uint32_t              ThreadPool::thread_count_;
uint32_t              ThreadPool::count_;
uint32_t              ThreadPool::block_size_;
uint32_t              ThreadPool::block_count_;
std::atomic<uint32_t> ThreadPool::block_index_;

std::mutex                              ThreadPool::mutex_;
std::condition_variable                 ThreadPool::sync_;
std::condition_variable                 ThreadPool::signal_;
std::unique_ptr<ThreadPool::KernelBase> ThreadPool::kernel_;
std::vector<std::thread>                ThreadPool::threads_;

uint32_t ThreadPool::GetThreadCount()
{
    return (uint32_t)threads_.size();
}

bool ThreadPool::Create(uint32_t thread_count)
{
    terminate_ = false;

    thread_count_ = 0;

    thread_count = (std::max(thread_count, 1u) + 1u) & ~1u;

    threads_.reserve(thread_count);

    // Spawn requested number of threads
    for (size_t i = 0; i < threads_.capacity(); ++i)
    {
        threads_.push_back(std::thread(Worker));
    }

    // Wait for all threads to have started
    for (;;)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (thread_count_ == threads_.size()) break; // all threads have started
        sync_.wait(lock);
    }

    return true;
}

void ThreadPool::Destroy()
{
    terminate_ = true;

    // Wait for all threads to have completed
    if (threads_.size() > 0)
    {
        signal_.notify_all();

        for (size_t i = 0; i < threads_.size(); ++i)
        {
            threads_[i].join();
        }
    }

    threads_.resize(0);

    thread_count_ = 0;
}

void ThreadPool::Worker()
{
    for (;;)
    {
        // Put the thread to sleep until some blocks need processing
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (++thread_count_ == threads_.capacity())
                sync_.notify_one(); // all threads are back to the pool
            signal_.wait(lock);
        }

        // Were we woken up to kill ourselves?
        if (terminate_) { break; }

        // Process all the available blocks
        {
            kernel_->Run();
        }
    }
}
} // namespace Capsaicin
