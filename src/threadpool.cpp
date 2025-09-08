#include "threadpool.h"


//线程池的实现


ThreadPool::~ThreadPool()
{
    try
    {
        stop();
    }
    catch (...)
    {
    }
}

void ThreadPool::start()
{
    if (running_)
    {
        throw std::runtime_error("thread pool already started");
    }
    running_ = true;

    for (int i = 0; i < initThreadSize_; ++i)
    {
        int tid = ++threadIdCounter_;
        auto th = std::make_unique<std::thread>(&ThreadPool::workerLoop, this, tid);
        threads_.emplace(tid, std::move(th));
        ++curThreadSize_;
        ++idleThreadSize_;
    }
}

void ThreadPool::stop()
{
    bool wasRunning = running_.exchange(false);
    if (!wasRunning)
    {

        return;
    }

    {
        std::lock_guard<std::mutex> lock(taskQueMtx_);
    }
    notEmpty_.notify_all();
    notFull_.notify_all();

    for (auto &kv : threads_)
    {
        if (kv.second && kv.second->joinable())
        {
            kv.second->join();
        }
    }
    threads_.clear();

    // 清理残留任务
    {
        std::lock_guard<std::mutex> lock(taskQueMtx_);
        while (!taskQue_.empty()) taskQue_.pop();
        taskSize_ = 0;
    }
}

void ThreadPool::workerLoop(int id)
{
    (void)id;
    while (true)
    {
        std::shared_ptr<Task> taskPtr;
        {
            std::unique_lock<std::mutex> lock(taskQueMtx_);
            if (mode_ == Thread_Mode::FIXED)
            {
                notEmpty_.wait(lock, [&]() {
                    return !taskQue_.empty() || !running_;
                });
                if (!running_ && taskQue_.empty())
                {
                    break;
                }
            }
            else
            {
                // CACHE 模式：带超时等待以便空闲回收
                if (!notEmpty_.wait_for(lock, std::chrono::milliseconds(kCacheIdleTimeoutMs), [&]() {
                        return !taskQue_.empty() || !running_;
                    }))
                {
                    // 超时，尝试回收
                    if (running_ && curThreadSize_ > initThreadSize_)
                    {
                        --curThreadSize_;
                        --idleThreadSize_;
                        return; // 直接退出该线程
                    }
                }
                if (!running_ && taskQue_.empty())
                {
                    break;
                }
                if (taskQue_.empty())
                {
                    // 被唤醒但没有任务（可能是超时或stop），继续循环判断
                    continue;
                }
            }

            // 取任务
            if (!taskQue_.empty())
            {
                taskPtr = taskQue_.front();
                taskQue_.pop();
                --taskSize_;
                // 队列有空间，通知可能阻塞在提交的线程
                notFull_.notify_one();
                --idleThreadSize_;
            }
        }

        if (taskPtr)
        {
            try
            {
                (*taskPtr)();
            }
            catch (...)
            {
                // 任务异常由 packaged_task 捕获；这里吞掉任何未包装的异常
            }
            ++idleThreadSize_;
        }

        // CACHE 模式扩容：当无空闲且仍有积压时，适度扩容
        if (mode_ == Thread_Mode::CACHE)
        {
            if (idleThreadSize_.load() == 0 && taskSize_.load() > 0 && curThreadSize_.load() < threadSizeThreadHold_)
            {
                int tid = ++threadIdCounter_;
                auto th = std::make_unique<std::thread>(&ThreadPool::workerLoop, this, tid);
                {
                    // 登记线程
                    threads_.emplace(tid, std::move(th));
                }
                ++curThreadSize_;
                ++idleThreadSize_;
            }
        }
    }
}
