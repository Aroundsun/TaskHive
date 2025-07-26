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
    // 构造函数
    ThreadPool(size_t thread_size, int flag = 0)
    {
        mode_ = flag == 0 ? Thread_Mode::FIXED : Thread_Mode::CACHE;
    }
    // 析构函数

    // 启动线程池
    void start()
    {
    }
    // 提交任务

    // 线程函数

private:
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
};