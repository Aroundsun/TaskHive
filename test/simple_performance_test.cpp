#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <numeric>
#include <algorithm>
#include <json/json.h>
#include "client.h"
#include "task.pb.h"

// 简化的性能测试类
class SimplePerformanceTest {
private:
    std::atomic<int64_t> total_tasks_submitted_{0};
    std::atomic<int64_t> total_tasks_completed_{0};
    std::atomic<int64_t> total_tasks_failed_{0};
    
    // 生成随机任务ID
    std::string generate_task_id() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(1000, 9999);
        return "simple-test-" + std::to_string(dis(gen));
    }
    
    // 创建简单的测试任务
    taskscheduler::Task create_simple_task() {
        taskscheduler::Task task;
        task.set_task_id(generate_task_id());
        task.set_type(taskscheduler::COMMAND);  // 使用命令类型
        task.set_content("echo 'Hello from performance test'");  // 简单的echo命令
        

        
        return task;
    }

public:
    // 测试任务提交性能
    void test_task_submission(int task_count = 100) {
        std::cout << "\n=== 任务提交性能测试 ===" << std::endl;
        std::cout << "测试任务数量: " << task_count << std::endl;
        
        Client client;
        client.start();
        
        std::vector<int64_t> submission_times;
        submission_times.reserve(task_count);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < task_count; ++i) {
            auto task_start = std::chrono::high_resolution_clock::now();
            
            try {
                taskscheduler::Task task = create_simple_task();
                client.submit_one_task(task);
                
                auto task_end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(task_end - task_start);
                submission_times.push_back(duration.count());
                
                total_tasks_submitted_++;
                std::cout << "✅ 任务提交成功: " << task.task_id() << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "❌ 任务提交失败: " << e.what() << std::endl;
                total_tasks_failed_++;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // 计算统计信息
        int64_t total_time = total_duration.count();
        int64_t avg_time = 0;
        int64_t min_time = 0;
        int64_t max_time = 0;
        
        if (!submission_times.empty()) {
            avg_time = std::accumulate(submission_times.begin(), submission_times.end(), 0LL) / submission_times.size();
            min_time = *std::min_element(submission_times.begin(), submission_times.end());
            max_time = *std::max_element(submission_times.begin(), submission_times.end());
        }
        
        double throughput = (total_tasks_submitted_ * 1000.0) / total_time;  // 任务/秒
        double success_rate = (total_tasks_submitted_ * 100.0) / task_count;
        
        std::cout << "\n📊 任务提交性能测试结果" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "总耗时: " << total_time << " ms" << std::endl;
        std::cout << "平均耗时: " << avg_time << " μs" << std::endl;
        std::cout << "最小耗时: " << min_time << " μs" << std::endl;
        std::cout << "最大耗时: " << max_time << " μs" << std::endl;
        std::cout << "成功提交任务数: " << total_tasks_submitted_ << std::endl;
        std::cout << "失败任务数: " << total_tasks_failed_ << std::endl;
        std::cout << "吞吐量: " << std::fixed << std::setprecision(2) << throughput << " tasks/s" << std::endl;
        std::cout << "成功率: " << std::fixed << std::setprecision(2) << success_rate << "%" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        
        client.stop();
    }
    
    // 测试并发任务提交
    void test_concurrent_submission(int concurrent_tasks = 50, int threads = 4) {
        std::cout << "\n=== 并发任务提交测试 ===" << std::endl;
        std::cout << "并发任务数: " << concurrent_tasks << ", 线程数: " << threads << std::endl;
        
        Client client;
        client.start();
        
        std::atomic<int> completed_tasks{0};
        std::atomic<int> failed_tasks{0};
        std::vector<std::thread> worker_threads;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 创建工作线程
        for (int t = 0; t < threads; ++t) {
            worker_threads.emplace_back([&, t]() {
                int tasks_per_thread = concurrent_tasks / threads;
                for (int i = 0; i < tasks_per_thread; ++i) {
                    try {
                        taskscheduler::Task task = create_simple_task();
                        client.submit_one_task(task);
                        completed_tasks++;
                    } catch (const std::exception& e) {
                        failed_tasks++;
                    }
                }
            });
        }
        
        // 等待所有线程完成
        for (auto& thread : worker_threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        int64_t total_time = total_duration.count();
        double throughput = (completed_tasks * 1000.0) / total_time;
        double success_rate = (completed_tasks * 100.0) / (completed_tasks + failed_tasks);
        
        std::cout << "\n📊 并发任务提交测试结果" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "总耗时: " << total_time << " ms" << std::endl;
        std::cout << "成功提交任务数: " << completed_tasks << std::endl;
        std::cout << "失败任务数: " << failed_tasks << std::endl;
        std::cout << "吞吐量: " << std::fixed << std::setprecision(2) << throughput << " tasks/s" << std::endl;
        std::cout << "成功率: " << std::fixed << std::setprecision(2) << success_rate << "%" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        
        client.stop();
    }
    
    // 测试系统连接性
    void test_system_connectivity() {
        std::cout << "\n=== 系统连接性测试 ===" << std::endl;
        
        Client client;
        
        try {
            client.start();
            std::cout << "✅ 客户端启动成功" << std::endl;
            
            // 测试Zookeeper连接
            try {
                auto scheduler_node = client.get_hearly_secheduler_node();
                std::cout << "✅ Zookeeper连接正常，找到调度器节点" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "⚠️  Zookeeper连接问题: " << e.what() << std::endl;
            }
            
            // 测试任务提交
            try {
                taskscheduler::Task task = create_simple_task();
                client.submit_one_task(task);
                std::cout << "✅ 任务提交功能正常" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "❌ 任务提交失败: " << e.what() << std::endl;
            }
            
            client.stop();
            std::cout << "✅ 客户端停止成功" << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "❌ 系统连接测试失败: " << e.what() << std::endl;
        }
    }
    
    // 运行所有测试
    void run_all_tests() {
        std::cout << "🚀 开始TaskHive系统简化性能测试" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        
        // 系统连接性测试
        test_system_connectivity();
        
        // 任务提交性能测试
        test_task_submission(50);
        
        // 并发任务提交测试
        test_concurrent_submission(100, 4);
        
        std::cout << "\n✅ 所有简化性能测试完成!" << std::endl;
    }
};

int main() {
    try {
        SimplePerformanceTest test;
        test.run_all_tests();
    } catch (const std::exception& e) {
        std::cerr << "性能测试失败: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 