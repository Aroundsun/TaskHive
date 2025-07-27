#include "thread_pool.h"
#include <iostream>
#include <chrono>
#include <random>
#include <string>
#include <vector>
#include <future>

// 简单的任务函数
void simple_task(int id) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "[Task " << id << "] 完成，线程ID: " 
              << std::this_thread::get_id() << std::endl;
}

// 返回值的任务函数
int calculate_task(int a, int b) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return a + b;
}

// 长时间运行的任务
void long_running_task(int id) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "[LongTask " << id << "] 完成" << std::endl;
}

// 自定义任务类
class CustomTask : public Task {
private:
    int task_id_;
    std::string message_;
    
public:
    CustomTask(int id, const std::string& msg) 
        : task_id_(id), message_(msg) {}
    
    void execute() override {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << "[CustomTask " << task_id_ << "] " << message_ 
                  << "，线程ID: " << std::this_thread::get_id() << std::endl;
    }
};

// 性能测试函数
void performance_test() {
    std::cout << "\n=== 性能测试 ===" << std::endl;
    
    const int task_count = 1000;
    const int thread_count = 4;
    
    ThreadPool pool(thread_count);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<std::future<void>> futures;
    for (int i = 0; i < task_count; ++i) {
        futures.push_back(pool.submit(simple_task, i));
    }
    
    // 等待所有任务完成
    for (auto& future : futures) {
        future.wait();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "完成 " << task_count << " 个任务，耗时: " 
              << duration.count() << "ms" << std::endl;
    std::cout << "平均每个任务耗时: " 
              << static_cast<double>(duration.count()) / task_count << "ms" << std::endl;
    
    pool.shutdown();
}

// 状态监控测试
void status_monitor_test() {
    std::cout << "\n=== 状态监控测试 ===" << std::endl;
    
    ThreadPool pool(2, 10); // 2个线程，最大队列大小10
    
    // 提交一些任务
    for (int i = 0; i < 5; ++i) {
        pool.submit(simple_task, i);
    }
    
    // 监控状态
    for (int i = 0; i < 10; ++i) {
        std::cout << "队列大小: " << pool.get_queue_size() 
                  << ", 已提交: " << pool.get_total_submitted()
                  << ", 已完成: " << pool.get_total_completed()
                  << ", 完成率: " << pool.get_completion_rate() << "%" << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    pool.shutdown();
}

// 异常处理测试
void exception_test() {
    std::cout << "\n=== 异常处理测试 ===" << std::endl;
    
    ThreadPool pool(2);
    
    // 提交一个会抛出异常的任务
    auto future = pool.submit([]() {
        throw std::runtime_error("测试异常");
    });
    
    try {
        future.get();
    } catch (const std::exception& e) {
        std::cout << "捕获到异常: " << e.what() << std::endl;
    }
    
    pool.shutdown();
}

// 动态调整测试
void resize_test() {
    std::cout << "\n=== 动态调整测试 ===" << std::endl;
    
    ThreadPool pool(2);
    
    std::cout << "初始线程数: " << pool.get_thread_count() << std::endl;
    
    // 增加线程数
    pool.resize(4);
    std::cout << "调整后线程数: " << pool.get_thread_count() << std::endl;
    
    // 提交一些任务
    for (int i = 0; i < 8; ++i) {
        pool.submit(simple_task, i);
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    pool.shutdown();
}

// 返回值测试
void return_value_test() {
    std::cout << "\n=== 返回值测试 ===" << std::endl;
    
    ThreadPool pool(4);
    
    std::vector<std::future<int>> futures;
    
    // 提交计算任务
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.submit(calculate_task, i, i * 2));
    }
    
    // 获取结果
    for (size_t i = 0; i < futures.size(); ++i) {
        int result = futures[i].get();
        std::cout << "任务 " << i << " 结果: " << result << std::endl;
    }
    
    pool.shutdown();
}

// 自定义任务测试
void custom_task_test() {
    std::cout << "\n=== 自定义任务测试 ===" << std::endl;
    
    ThreadPool pool(3);
    
    // 提交自定义任务
    for (int i = 0; i < 5; ++i) {
        auto task = std::make_unique<CustomTask>(i, "自定义消息 " + std::to_string(i));
        pool.submit_task(std::move(task));
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    pool.shutdown();
}

// 压力测试
void stress_test() {
    std::cout << "\n=== 压力测试 ===" << std::endl;
    
    ThreadPool pool(8, 100); // 8个线程，队列大小100
    
    const int task_count = 500;
    std::vector<std::future<int>> futures;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 提交大量任务
    for (int i = 0; i < task_count; ++i) {
        futures.push_back(pool.submit([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return i * i;
        }));
    }
    
    // 获取结果
    int sum = 0;
    for (auto& future : futures) {
        sum += future.get();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "压力测试完成，结果总和: " << sum 
              << "，耗时: " << duration.count() << "ms" << std::endl;
    
    pool.shutdown();
}

int main() {
    std::cout << "=== 线程池演示程序 ===" << std::endl;
    
    try {
        // 基本功能测试
        std::cout << "\n=== 基本功能测试 ===" << std::endl;
        {
            ThreadPool pool(4);
            
            // 提交简单任务
            for (int i = 0; i < 8; ++i) {
                pool.submit(simple_task, i);
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
            pool.shutdown();
        }
        
        // 运行各种测试
        performance_test();
        status_monitor_test();
        exception_test();
        resize_test();
        return_value_test();
        custom_task_test();
        stress_test();
        
        std::cout << "\n=== 所有测试完成 ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "程序异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 