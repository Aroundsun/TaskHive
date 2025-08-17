#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

#include "AsyncBuffer.hpp"
namespace logsystem
{
    enum class AsyncType
    {
        BLOCKING_BOUNDED, // 固定容量，写不下就阻塞
        NONBLOCKING_GROW  // 可扩容，优先吞吐
    };
    using functor = std::function<void(logsystem::Buffer &)>; // 定义一个函数对象类型，用于处理日志缓冲区
    // 异步工作线程类
    class AsyncWorker
    {
    public:
        AsyncWorker(const logsystem::functor &cb, logsystem::AsyncType at = logsystem::AsyncType::BLOCKING_BOUNDED)
            : callback_(cb), async_type_(at), thread_(&logsystem::AsyncWorker::ThreadFunc, this), stop_(false)
        {
        }

        ~AsyncWorker()
        {
            Stop();
        }
        void Stop()
        {
            stop_ = true;                // 设置停止标志
            cond_consumer_.notify_all(); // 通知消费者线程停止
            thread_.join();              // 等待线程结束
        }
        // 向生产者缓冲区推送数据
        void Push(const char *data, size_t len)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stop_)
            {
                std::cerr << "Push: 异步工作器已停止，无法推送数据" << std::endl;
                return;
            }
            if (async_type_ == logsystem::AsyncType::BLOCKING_BOUNDED)
            {
                // 如果是阻塞有界缓冲区，等待直到缓冲区有足够的空间
                cond_productor_.wait(lock, [this, len]()
                                     { return buffer_productor_.WriteableSize() >= len; });
                if (stop_)
                {
                    std::cerr << "Push: 异步工作器已停止，无法推送数据" << std::endl;
                    return;
                }
            }
            buffer_productor_.Push(data, len); // 将数据写入生产者缓冲区
            // lock.unlock(); // 手动解锁互斥锁，避免消费者线程在刚被唤醒又被阻塞
            cond_consumer_.notify_one(); // 通知消费者线程有新数据可处理
        }

    private:
        // 异步工作器的线程函数
        void ThreadFunc()
        {
            while (true)
            {
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    if (buffer_productor_.isEmpty() && !stop_)
                    {
                        cond_consumer_.wait(lock, [this]()
                                            { return stop_ || !buffer_productor_.isEmpty(); });
                    }

                    buffer_productor_.Swap(buff_consumer_); // 交换生产者和消费者缓冲区
                    // 固定容量的缓冲区才需要唤醒
                    if (async_type_ == logsystem::AsyncType::BLOCKING_BOUNDED)
                        cond_productor_.notify_one();
                }
                callback_(buff_consumer_); // 调用回调函数处理消费者缓冲区的数据
                buff_consumer_.reset();    // 重置消费者缓冲区
                if (stop_ && buffer_productor_.isEmpty())
                {
                    return; // 如果停止标志为真且生产者缓冲区为空，退出线程
                }
            }
        }

    private:
        logsystem::AsyncType async_type_;        // 异步类型
        std::atomic<bool> stop_;                 // 用于控制异步工作器的启动
        std::mutex mutex_;                       // 互斥锁，保护缓冲区和条件变量
        logsystem::Buffer buffer_productor_;     // 生产者缓冲区
        logsystem::Buffer buff_consumer_;        // 消费者缓冲区
        std::condition_variable cond_productor_; // 生产者条件变量
        std::condition_variable cond_consumer_;  // 消费者条件变量
        std::thread thread_;                     // 异步工作器的线程
        logsystem::functor callback_;            // 回调函数
    };
} // namespace logsystem