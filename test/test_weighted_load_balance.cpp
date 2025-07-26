#include "client.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "=== 权重配置负载均衡测试 ===" << std::endl;
    
    try {
        // 创建客户端实例
        Client client;
        
        // 启动客户端
        client.start();
        
        // 等待客户端初始化
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        std::cout << "\n当前负载均衡策略: " << static_cast<int>(client.get_load_balance_strategy()) << std::endl;
        
        // 测试综合评分策略（使用配置文件中的权重）
        std::cout << "\n=== 测试综合评分策略（使用配置文件权重）===" << std::endl;
        
        // 提交多个任务来测试权重计算
        for(int i = 0; i < 5; ++i) {
            taskscheduler::Task task;
            task.set_task_id("weight_test_task_" + std::to_string(i+1));
            task.set_type(taskscheduler::TaskType::FUNCTION);
            task.set_content("add_numbers");
            task.mutable_metadata()->insert({"a", std::to_string(i*10)});
            task.mutable_metadata()->insert({"b", std::to_string(i*10 + 5)});
            
            client.submit_one_task(task);
            std::cout << "提交任务: " << task.task_id() << std::endl;
            
            // 等待一段时间让任务被处理
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        
        std::cout << "\n任务提交完成，等待结果..." << std::endl;
        
        // 等待任务执行
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // 查询结果
        for(int i = 0; i < 5; ++i) {
            std::string task_id = "weight_test_task_" + std::to_string(i+1);
            taskscheduler::TaskResult result = client.get_task_result(task_id);
            if(result.task_id() != "") {
                std::cout << "任务 " << task_id << " 结果: " << result.output() 
                          << ", 状态: " << result.status() << std::endl;
            } else {
                std::cout << "任务 " << task_id << " 尚未完成" << std::endl;
            }
        }
        
        std::cout << "\n=== 权重配置测试完成 ===" << std::endl;
        std::cout << "请查看日志中的权重计算信息：" << std::endl;
        std::cout << "  - '[LoadBalance] 计算评分 - CPU:X(Y) 内存:X(Y) 磁盘:X(Y) 网络:X(Y) 负载:X(Y) 总分:Z'" << std::endl;
        std::cout << "  - 其中Y是配置文件中的权重值" << std::endl;
        std::cout << "\n你可以修改 config/client_config.json 中的 load_balance_weights 来调整权重：" << std::endl;
        std::cout << "  - cpu_weight: CPU权重" << std::endl;
        std::cout << "  - memory_weight: 内存权重" << std::endl;
        std::cout << "  - disk_weight: 磁盘权重" << std::endl;
        std::cout << "  - network_weight: 网络权重" << std::endl;
        std::cout << "  - load_weight: 负载权重" << std::endl;
        
    } catch(const std::exception& e) {
        std::cerr << "测试失败: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 