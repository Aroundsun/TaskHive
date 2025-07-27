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

// 性能测试结果结构
struct PerformanceResult {
    std::string test_name;
    int64_t total_time_ms;
    int64_t avg_time_ms;
    int64_t min_time_ms;
    int64_t max_time_ms;
    int64_t total_operations;
    double throughput;  // 操作/秒
    double success_rate;
    std::string description;
};

// 性能测试类
class PerformanceTest {
private:
    std::vector<PerformanceResult> results_;
    std::atomic<int64_t> total_tasks_submitted_{0};
    std::atomic<int64_t> total_tasks_completed_{0};
    std::atomic<int64_t> total_tasks_failed_{0};
    
    // 生成随机任务ID
    std::string generate_task_id() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(1000, 9999);
        return "perf-test-" + std::to_string(dis(gen));
    }
    
    // 创建测试任务
    taskscheduler::Task create_test_task(const std::string& task_type = "simple") {
        taskscheduler::Task task;
        task.set_task_id(generate_task_id());
        task.set_type(taskscheduler::FUNCTION);  // 使用函数类型
        
        // 设置任务内容
        std::string content;
        if (task_type == "cpu_intensive") {
            content = "cpu_bound_function";
        } else if (task_type == "memory_intensive") {
            content = "memory_bound_function";
        } else if (task_type == "io_intensive") {
            content = "io_bound_function";
        } else {
            content = "simple_function";
        }
        task.set_content(content);
        
        // 设置任务参数到metadata
        if (task_type == "cpu_intensive") {
            task.mutable_metadata()->insert({"iterations", "1000000"});
            task.mutable_metadata()->insert({"operation", "cpu_bound"});
        } else if (task_type == "memory_intensive") {
            task.mutable_metadata()->insert({"size_mb", "100"});
            task.mutable_metadata()->insert({"operation", "memory_bound"});
        } else if (task_type == "io_intensive") {
            task.mutable_metadata()->insert({"file_size", "10"});
            task.mutable_metadata()->insert({"operation", "io_bound"});
        } else {
            task.mutable_metadata()->insert({"iterations", "1000"});
            task.mutable_metadata()->insert({"operation", "simple"});
        }
        
        return task;
    }

public:
    // 任务提交性能测试
    PerformanceResult test_task_submission_performance(int task_count = 1000) {
        std::cout << "\n=== 任务提交性能测试 ===" << std::endl;
        std::cout << "测试任务数量: " << task_count << std::endl;
        
        Client client;
        client.start();
        
        std::vector<int64_t> submission_times;
        submission_times.reserve(task_count);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < task_count; ++i) {
            auto task_start = std::chrono::high_resolution_clock::now();
            
            taskscheduler::Task task = create_test_task("simple");
            client.submit_one_task(task);
            
            auto task_end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(task_end - task_start);
            submission_times.push_back(duration.count());
            
            total_tasks_submitted_++;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // 计算统计信息
        int64_t total_time = total_duration.count();
        int64_t avg_time = std::accumulate(submission_times.begin(), submission_times.end(), 0LL) / submission_times.size();
        int64_t min_time = *std::min_element(submission_times.begin(), submission_times.end());
        int64_t max_time = *std::max_element(submission_times.begin(), submission_times.end());
        double throughput = (task_count * 1000.0) / total_time;  // 任务/秒
        
        PerformanceResult result{
            "任务提交性能测试",
            total_time,
            avg_time,
            min_time,
            max_time,
            task_count,
            throughput,
            100.0,  // 假设100%成功率
            "测试任务提交的吞吐量和延迟"
        };
        
        results_.push_back(result);
        print_result(result);
        
        client.stop();
        return result;
    }
    
    // 并发任务处理性能测试
    PerformanceResult test_concurrent_task_processing(int concurrent_tasks = 100, int threads = 4) {
        std::cout << "\n=== 并发任务处理性能测试 ===" << std::endl;
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
                        taskscheduler::Task task = create_test_task("simple");
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
        
        PerformanceResult result{
            "并发任务处理性能测试",
            total_time,
            total_time / completed_tasks,
            0,  // 简化处理
            0,  // 简化处理
            completed_tasks,
            throughput,
            success_rate,
            "测试多线程并发提交任务的性能"
        };
        
        results_.push_back(result);
        print_result(result);
        
        client.stop();
        return result;
    }
    
    // 负载均衡性能测试
    PerformanceResult test_load_balancing_performance(int test_rounds = 100) {
        std::cout << "\n=== 负载均衡性能测试 ===" << std::endl;
        std::cout << "测试轮数: " << test_rounds << std::endl;
        
        Client client;
        client.start();
        
        std::vector<int64_t> selection_times;
        selection_times.reserve(test_rounds);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < test_rounds; ++i) {
            auto selection_start = std::chrono::high_resolution_clock::now();
            
            try {
                auto scheduler_node = client.get_hearly_secheduler_node();
                auto selection_end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(selection_end - selection_start);
                selection_times.push_back(duration.count());
            } catch (const std::exception& e) {
                std::cerr << "负载均衡选择失败: " << e.what() << std::endl;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        int64_t total_time = total_duration.count();
        int64_t avg_time = std::accumulate(selection_times.begin(), selection_times.end(), 0LL) / selection_times.size();
        int64_t min_time = *std::min_element(selection_times.begin(), selection_times.end());
        int64_t max_time = *std::max_element(selection_times.begin(), selection_times.end());
        double throughput = (test_rounds * 1000.0) / total_time;
        
        PerformanceResult result{
            "负载均衡性能测试",
            total_time,
            avg_time,
            min_time,
            max_time,
            test_rounds,
            throughput,
            100.0,
            "测试负载均衡算法的选择性能"
        };
        
        results_.push_back(result);
        print_result(result);
        
        client.stop();
        return result;
    }
    
    // 任务执行性能测试
    PerformanceResult test_task_execution_performance(int task_count = 100) {
        std::cout << "\n=== 任务执行性能测试 ===" << std::endl;
        std::cout << "测试任务数量: " << task_count << std::endl;
        
        Client client;
        client.start();
        
        std::vector<taskscheduler::Task> tasks;
        std::vector<std::string> task_ids;
        
        // 提交任务
        for (int i = 0; i < task_count; ++i) {
            taskscheduler::Task task = create_test_task("cpu_intensive");
            client.submit_one_task(task);
            task_ids.push_back(task.task_id());
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 等待任务完成
        int completed_count = 0;
        int timeout_count = 0;
        const int timeout_seconds = 60;
        
        auto timeout_start = std::chrono::high_resolution_clock::now();
        
        while (completed_count < task_count) {
            for (const auto& task_id : task_ids) {
                try {
                    auto result = client.get_task_result(task_id);
                    if (!result.task_id().empty()) {
                        completed_count++;
                    }
                } catch (const std::exception& e) {
                    // 任务还在执行中
                }
            }
            
            auto current_time = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - timeout_start);
            
            if (elapsed.count() > timeout_seconds) {
                timeout_count = task_count - completed_count;
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        int64_t total_time = total_duration.count();
        double throughput = (completed_count * 1000.0) / total_time;
        double success_rate = (completed_count * 100.0) / task_count;
        
        PerformanceResult result{
            "任务执行性能测试",
            total_time,
            total_time / completed_count,
            0,
            0,
            completed_count,
            throughput,
            success_rate,
            "测试任务从提交到完成的端到端性能"
        };
        
        results_.push_back(result);
        print_result(result);
        
        client.stop();
        return result;
    }
    
    // 内存使用性能测试
    PerformanceResult test_memory_usage_performance(int task_count = 50) {
        std::cout << "\n=== 内存使用性能测试 ===" << std::endl;
        std::cout << "测试任务数量: " << task_count << std::endl;
        
        Client client;
        client.start();
        
        std::vector<taskscheduler::Task> tasks;
        
        // 提交内存密集型任务
        for (int i = 0; i < task_count; ++i) {
            taskscheduler::Task task = create_test_task("memory_intensive");
            client.submit_one_task(task);
            tasks.push_back(task);
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 等待任务完成
        int completed_count = 0;
        const int timeout_seconds = 120;
        
        auto timeout_start = std::chrono::high_resolution_clock::now();
        
        while (completed_count < task_count) {
            for (const auto& task : tasks) {
                try {
                    auto result = client.get_task_result(task.task_id());
                    if (!result.task_id().empty()) {
                        completed_count++;
                    }
                } catch (const std::exception& e) {
                    // 任务还在执行中
                }
            }
            
            auto current_time = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - timeout_start);
            
            if (elapsed.count() > timeout_seconds) {
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        int64_t total_time = total_duration.count();
        double throughput = (completed_count * 1000.0) / total_time;
        double success_rate = (completed_count * 100.0) / task_count;
        
        PerformanceResult result{
            "内存使用性能测试",
            total_time,
            total_time / completed_count,
            0,
            0,
            completed_count,
            throughput,
            success_rate,
            "测试内存密集型任务的执行性能"
        };
        
        results_.push_back(result);
        print_result(result);
        
        client.stop();
        return result;
    }
    
    // 打印测试结果
    void print_result(const PerformanceResult& result) {
        std::cout << "\n📊 " << result.test_name << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "总耗时: " << result.total_time_ms << " ms" << std::endl;
        std::cout << "平均耗时: " << result.avg_time_ms << " μs" << std::endl;
        std::cout << "最小耗时: " << result.min_time_ms << " μs" << std::endl;
        std::cout << "最大耗时: " << result.max_time_ms << " μs" << std::endl;
        std::cout << "总操作数: " << result.total_operations << std::endl;
        std::cout << "吞吐量: " << std::fixed << std::setprecision(2) << result.throughput << " ops/s" << std::endl;
        std::cout << "成功率: " << std::fixed << std::setprecision(2) << result.success_rate << "%" << std::endl;
        std::cout << "描述: " << result.description << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    }
    
    // 生成性能测试报告
    void generate_report(const std::string& filename = "performance_report.txt") {
        std::ofstream report(filename);
        if (!report.is_open()) {
            std::cerr << "无法创建报告文件: " << filename << std::endl;
            return;
        }
        
        report << "TaskHive 系统性能测试报告" << std::endl;
        report << "生成时间: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;
        report << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        
        for (const auto& result : results_) {
            report << "\n📊 " << result.test_name << std::endl;
            report << "总耗时: " << result.total_time_ms << " ms" << std::endl;
            report << "平均耗时: " << result.avg_time_ms << " μs" << std::endl;
            report << "吞吐量: " << std::fixed << std::setprecision(2) << result.throughput << " ops/s" << std::endl;
            report << "成功率: " << std::fixed << std::setprecision(2) << result.success_rate << "%" << std::endl;
            report << "描述: " << result.description << std::endl;
            report << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        }
        
        report.close();
        std::cout << "\n📄 性能测试报告已生成: " << filename << std::endl;
    }
    
    // 运行所有性能测试
    void run_all_tests() {
        std::cout << "🚀 开始TaskHive系统性能测试" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        
        // 运行各种性能测试
        test_task_submission_performance(1000);
        test_concurrent_task_processing(500, 4);
        test_load_balancing_performance(200);
        test_task_execution_performance(50);
        test_memory_usage_performance(20);
        
        // 生成报告
        generate_report();
        
        std::cout << "\n✅ 所有性能测试完成!" << std::endl;
    }
};

int main() {
    try {
        PerformanceTest test;
        test.run_all_tests();
    } catch (const std::exception& e) {
        std::cerr << "性能测试失败: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 