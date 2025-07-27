#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <atomic>

// 任务接口基类
class Task {
public:
    virtual ~Task() = default;
    virtual void execute() = 0;
};

// 函数任务包装器
template<typename F>
class FunctionTask : public Task {
private:
    F func_;
    std::promise<void> promise_;

public:
    FunctionTask(F&& func) : func_(std::forward<F>(func)) {}
    
    void execute() override {
        try {
            func_();
            promise_.set_value();
        } catch (...) {
            promise_.set_exception(std::current_exception());
        }
    }
    
    std::future<void> get_future() {
        return promise_.get_future();
    }
};

// 线程池类
class ThreadPool {
private:
    // 线程池状态
    std::atomic<bool> stop_;
    std::atomic<bool> shutdown_;
    
    // 线程相关
    std::vector<std::thread> workers_;
    std::queue<std::unique_ptr<Task>> tasks_;
    
    // 同步原语
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    
    // 统计信息
    std::atomic<size_t> total_tasks_submitted_;
    std::atomic<size_t> total_tasks_completed_;
    std::atomic<size_t> current_queue_size_;
    
    // 配置参数
    size_t thread_count_;
    size_t max_queue_size_;
    
    // 工作线程函数
    void worker_function();

public:
    // 构造函数
    explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency(),
                       size_t max_queue_size = 1000);
    
    // 析构函数
    ~ThreadPool();
    
    // 禁用拷贝构造和赋值
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    // 提交任务接口
    template<class F, class... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    // 提交Task对象
    void submit_task(std::unique_ptr<Task> task);
    
    // 线程池控制
    void shutdown();
    void shutdown_now();
    bool is_shutdown() const;
    
    // 状态查询
    size_t get_thread_count() const;
    size_t get_queue_size() const;
    size_t get_total_submitted() const;
    size_t get_total_completed() const;
    double get_completion_rate() const;
    
    // 等待所有任务完成
    void wait_for_all();
    
    // 清空任务队列
    void clear_queue();
    
    // 动态调整线程数
    void resize(size_t new_thread_count);
};

// 模板方法实现
template<class F, class... Args>
auto ThreadPool::submit(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    using return_type = typename std::result_of<F(Args...)>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> res = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        // 检查是否已关闭
        if (stop_ || shutdown_) {
            throw std::runtime_error("线程池已关闭，无法提交任务");
        }
        
        // 检查队列是否已满
        if (max_queue_size_ > 0 && tasks_.size() >= max_queue_size_) {
            throw std::runtime_error("任务队列已满");
        }
        
        // 创建任务包装器
        auto task_wrapper = std::make_unique<FunctionTask<void()>>([task]() {
            (*task)();
        });
        
        tasks_.emplace(std::move(task_wrapper));
        total_tasks_submitted_++;
        current_queue_size_ = tasks_.size();
    }
    
    condition_.notify_one();
    return res;
}

#endif // THREAD_POOL_H 