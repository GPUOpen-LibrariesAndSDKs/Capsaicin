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
#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Capsaicin
{
class ThreadPool
{
public:
    inline ThreadPool() {}

    template<typename KERNEL>
    void Dispatch(KERNEL const &kernel, uint32_t count, uint32_t block_size = 16) const
    {
        // Set dispatch properties
        count_       = count;
        block_size_  = std::max(block_size, 1u);
        block_count_ = (count_ + block_size_ - 1) / block_size_;

        // Special case - only 1 block? no need to go wide
        if (block_count_ <= 1)
        {
            for (uint32_t i = 0; i < count; ++i)
            {
                kernel(i);
            }
        }
        else
        {
            // Install the kernel
            block_index_ = 0;
            kernel_      = std::make_unique<Kernel<KERNEL>>(kernel);

            // And dispatch
            thread_count_ = 0; // consume all threads from the pool
            signal_.notify_all();

            // Wait until all threads have returned to the pool
            for (;;)
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (thread_count_ == threads_.size()) break; // all threads have completed
                sync_.wait(lock);
            }

            // Release kernel
            kernel_.reset();
        }
    }

    static uint32_t GetThreadCount();

    static bool Create(uint32_t thread_count);
    static void Destroy();

protected:
    class KernelBase
    {
        KernelBase(KernelBase const &)            = delete;
        KernelBase &operator=(KernelBase const &) = delete;

    public:
        inline KernelBase() {}

        inline virtual ~KernelBase() {}

        inline virtual void Run() = 0 {}
    };

    template<typename KERNEL>
    class Kernel : public KernelBase
    {
    public:
        Kernel(KERNEL const &kernel)
            : kernel_(kernel)
        {}

        void Run() override
        {
            for (;;)
            {
                uint32_t const block_index = block_index_++;

                if (block_index >= block_count_)
                {
                    break; // everything has been processed
                }

                uint32_t const begin = block_index * block_size_;

                uint32_t const end = std::min((block_index + 1) * block_size_, count_);

                for (uint32_t index = begin; index < end; ++index)
                {
                    kernel_(index); // run the kernel
                }
            }
        }

    protected:
        KERNEL const &kernel_; /**< The kernel to be executed.*/
    };

    static void Worker();

    static bool                  terminate_;    /**< Whether to terminate the threads. */
    static uint32_t              thread_count_; /**< The number of available threads. */
    static uint32_t              count_;        /**< The number of kernel invocations. */
    static uint32_t              block_size_;   /**< The size of a block. */
    static uint32_t              block_count_;  /**< The number of available blocks. */
    static std::atomic<uint32_t> block_index_;  /**< The index of the currently executed block. */

    static std::mutex                  mutex_;   /**< The mutex for synchronization. */
    static std::condition_variable     sync_;    /**< The condition variable for synchronizing. */
    static std::condition_variable     signal_;  /**< The condition variable for signalling. */
    static std::unique_ptr<KernelBase> kernel_;  /**< The kernel scheduled for execution. */
    static std::vector<std::thread>    threads_; /**< The available CPU threads. */
};
} // namespace Capsaicin
