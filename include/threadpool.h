#pragma once
#include <thread>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <future>
#include <functional>
#include <atomic>

#include <unordered_map>
#include <queue>
/*
  暂时不集成线程池，需要完善优化原有的功能
*/
enum Thread_Mode
{
    FIXED = 0,
    CACHE = 1,
};

// 线程池配置
const size_t max_threads_size = 4;

class ThreadPool
{
public:
    using Task = std::function<void()>;

    ThreadPool(size_t thread_size, int flag = 0)
    {
        mode_ = flag == 0 ? Thread_Mode::FIXED : Thread_Mode::CACHE;
        initThreadSize_ = static_cast<int>(thread_size);
        taskQueMaxThreshold_ = 1024;
        threadSizeThreadHold_ = static_cast<int>(max_threads_size);
        curThreadSize_ = 0;
        idleThreadSize_ = 0;
        taskSize_ = 0;
        running_ = false;
        threadIdCounter_ = 0;
    }


    ThreadPool(size_t thread_size, Thread_Mode mode, size_t task_queue_max, size_t thread_max)
    {
        mode_ = mode;
        initThreadSize_ = static_cast<int>(thread_size);
        taskQueMaxThreshold_ = static_cast<int>(task_queue_max);
        threadSizeThreadHold_ = static_cast<int>(thread_max);
        curThreadSize_ = 0;
        idleThreadSize_ = 0;
        taskSize_ = 0;
        running_ = false;
        threadIdCounter_ = 0;
    }

    // 析构函数
    ~ThreadPool();

    // 启动线程池（只允许调用一次）
    void start();

    // 停止线程池（优雅关闭，排空队列，等待所有线程退出）
    void stop();

    // 提交任务（队列满会阻塞等待）
    template <class F, class... Args>
    auto submit(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>
    {
        using R = typename std::invoke_result<F, Args...>::type;

        if (!running_)
        {
            throw std::runtime_error("thread pool not running");
        }

        auto bound = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        auto pkg = std::make_shared<std::packaged_task<R()>>(std::move(bound));
        std::future<R> fut = pkg->get_future();

        std::unique_lock<std::mutex> lock(taskQueMtx_);
        notFull_.wait(lock, [&]() { return taskSize_ < static_cast<unsigned int>(taskQueMaxThreshold_) || !running_; });
        if (!running_)
        {
            throw std::runtime_error("thread pool stopped");
        }
        taskQue_.emplace(std::make_shared<Task>([pkg]() { (*pkg)(); }));
        ++taskSize_;
        lock.unlock();
        notEmpty_.notify_one();
        return fut;
    }

    // 提交任务（队列满等待至超时）
    template <class F, class... Args>
    auto submit_for(uint64_t timeout_ms, F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>
    {
        using R = typename std::invoke_result<F, Args...>::type;

        if (!running_)
        {
            throw std::runtime_error("thread pool not running");
        }

        auto bound = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        auto pkg = std::make_shared<std::packaged_task<R()>>(std::move(bound));
        std::future<R> fut = pkg->get_future();

        std::unique_lock<std::mutex> lock(taskQueMtx_);
        if (!notFull_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&]() { return taskSize_ < static_cast<unsigned int>(taskQueMaxThreshold_) || !running_; }))
        {
            throw std::runtime_error("submit timeout");
        }
        if (!running_)
        {
            throw std::runtime_error("thread pool stopped");
        }
        taskQue_.emplace(std::make_shared<Task>([pkg]() { (*pkg)(); }));
        ++taskSize_;
        lock.unlock();
        notEmpty_.notify_one();
        return fut;
    }

    // 查询接口
    size_t pending() const noexcept { return taskSize_.load(); }
    size_t size() const noexcept { return curThreadSize_.load(); }
    size_t idle() const noexcept { return idleThreadSize_.load(); }

private:
    void workerLoop(int id);

    static constexpr uint64_t kCacheIdleTimeoutMs = 60000;
    // std::vector<std::unique_ptr<Thread> > threads_;   //线程列表
    std::unordered_map<int, std::unique_ptr<std::thread>> threads_; // 线程列表
    int initThreadSize_;                                       // 初始的线程数量
    int threadSizeThreadHold_;                                 // cached 模式下线程数量的上限阈值
    std::atomic_int curThreadSize_;                            // 记录当前线程池中的线程总数量
    std::atomic_int idleThreadSize_;                           // 记录空闲线程的数量

    std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列
    std::atomic_uint taskSize_;                 // 任务的数量
    int taskQueMaxThreshold_;                   // 任务队列的上限阈值

    std::mutex taskQueMtx_;            // 保证任务队列的线程安全
    std::condition_variable notFull_;  // 表示任务队列不满
    std::condition_variable notEmpty_; // 表示任务队列不空
    std::condition_variable exitCond_; // 等到线程资源全部回收

    Thread_Mode mode_;         // 当前线程池的工作模式
    std::atomic_bool running_; // 表示线程池的启动状态

    int threadIdCounter_;
};