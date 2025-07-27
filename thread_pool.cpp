#include "thread_pool.h"
#include <iostream>
#include <chrono>
#include <algorithm>

// 构造函数
ThreadPool::ThreadPool(size_t thread_count, size_t max_queue_size)
    : stop_(false)
    , shutdown_(false)
    , total_tasks_submitted_(0)
    , total_tasks_completed_(0)
    , current_queue_size_(0)
    , thread_count_(thread_count)
    , max_queue_size_(max_queue_size) {
    
    if (thread_count == 0) {
        thread_count_ = std::thread::hardware_concurrency();
    }
    
    std::cout << "[ThreadPool] 创建线程池，线程数: " << thread_count_ 
              << "，最大队列大小: " << max_queue_size_ << std::endl;
    
    // 创建工作线程
    for (size_t i = 0; i < thread_count_; ++i) {
        workers_.emplace_back(&ThreadPool::worker_function, this);
    }
}

// 析构函数
ThreadPool::~ThreadPool() {
    shutdown();
}

// 工作线程函数
void ThreadPool::worker_function() {
    while (true) {
        std::unique_ptr<Task> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            // 等待任务或停止信号
            condition_.wait(lock, [this] {
                return stop_ || shutdown_ || !tasks_.empty();
            });
            
            // 如果线程池停止且队列为空，退出
            if ((stop_ || shutdown_) && tasks_.empty()) {
                return;
            }
            
            // 获取任务
            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
                current_queue_size_ = tasks_.size();
            }
        }
        
        // 执行任务
        if (task) {
            try {
                task->execute();
                total_tasks_completed_++;
            } catch (const std::exception& e) {
                std::cerr << "[ThreadPool] 任务执行异常: " << e.what() << std::endl;
            }
        }
    }
}

// 提交Task对象
void ThreadPool::submit_task(std::unique_ptr<Task> task) {
    if (!task) {
        throw std::invalid_argument("任务不能为空");
    }
    
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
        
        tasks_.emplace(std::move(task));
        total_tasks_submitted_++;
        current_queue_size_ = tasks_.size();
    }
    
    condition_.notify_one();
}

// 优雅关闭
void ThreadPool::shutdown() {
    if (shutdown_) {
        return;
    }
    
    std::cout << "[ThreadPool] 开始优雅关闭..." << std::endl;
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        shutdown_ = true;
    }
    
    // 通知所有等待的线程
    condition_.notify_all();
    
    // 等待所有工作线程结束
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    std::cout << "[ThreadPool] 优雅关闭完成" << std::endl;
}

// 立即关闭
void ThreadPool::shutdown_now() {
    if (stop_) {
        return;
    }
    
    std::cout << "[ThreadPool] 开始立即关闭..." << std::endl;
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
        shutdown_ = true;
        
        // 清空任务队列
        while (!tasks_.empty()) {
            tasks_.pop();
        }
        current_queue_size_ = 0;
    }
    
    // 通知所有等待的线程
    condition_.notify_all();
    
    // 等待所有工作线程结束
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    std::cout << "[ThreadPool] 立即关闭完成" << std::endl;
}

// 检查是否已关闭
bool ThreadPool::is_shutdown() const {
    return shutdown_ || stop_;
}

// 获取线程数
size_t ThreadPool::get_thread_count() const {
    return thread_count_;
}

// 获取队列大小
size_t ThreadPool::get_queue_size() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

// 获取总提交任务数
size_t ThreadPool::get_total_submitted() const {
    return total_tasks_submitted_;
}

// 获取总完成任务数
size_t ThreadPool::get_total_completed() const {
    return total_tasks_completed_;
}

// 获取完成率
double ThreadPool::get_completion_rate() const {
    size_t submitted = total_tasks_submitted_;
    size_t completed = total_tasks_completed_;
    
    if (submitted == 0) {
        return 0.0;
    }
    
    return static_cast<double>(completed) / static_cast<double>(submitted) * 100.0;
}

// 等待所有任务完成
void ThreadPool::wait_for_all() {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (tasks_.empty()) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// 清空任务队列
void ThreadPool::clear_queue() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    size_t cleared_count = tasks_.size();
    while (!tasks_.empty()) {
        tasks_.pop();
    }
    current_queue_size_ = 0;
    
    std::cout << "[ThreadPool] 清空队列，移除了 " << cleared_count << " 个任务" << std::endl;
}

// 动态调整线程数
void ThreadPool::resize(size_t new_thread_count) {
    if (new_thread_count == 0) {
        new_thread_count = std::thread::hardware_concurrency();
    }
    
    std::cout << "[ThreadPool] 调整线程数从 " << thread_count_ 
              << " 到 " << new_thread_count << std::endl;
    
    if (new_thread_count > thread_count_) {
        // 增加线程
        size_t add_count = new_thread_count - thread_count_;
        for (size_t i = 0; i < add_count; ++i) {
            workers_.emplace_back(&ThreadPool::worker_function, this);
        }
    } else if (new_thread_count < thread_count_) {
        // 减少线程 - 通过设置停止标志
        // 注意：这是一个简化的实现，实际应用中可能需要更复杂的线程管理
        std::cout << "[ThreadPool] 警告：减少线程数的功能需要更复杂的实现" << std::endl;
    }
    
    thread_count_ = new_thread_count;
} 