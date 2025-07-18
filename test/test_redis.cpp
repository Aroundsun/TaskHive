#include "redis_client.h"
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <vector>

void test_basic_operations(RedisClient& redis) {
    std::cout << "=== 测试基本操作 ===" << std::endl;
    
    // 测试 setTaskResult/getTaskResult
    std::string task_id = "test_task_001";
    std::string result = "{\"status\":\"success\",\"output\":\"Hello World\",\"timestamp\":1234567890}";
    
    if (redis.setTaskResult(task_id, result, 60)) {
        std::cout << "✓ setTaskResult 成功" << std::endl;
    } else {
        std::cout << "✗ setTaskResult 失败" << std::endl;
        return;
    }

    std::string get_result = redis.getTaskResult(task_id);
    if (!get_result.empty()) {
        std::cout << "✓ getTaskResult 成功: " << get_result << std::endl;
    } else {
        std::cout << "✗ getTaskResult 失败" << std::endl;
    }
}

void test_multiple_tasks(RedisClient& redis) {
    std::cout << "\n=== 测试多个任务 ===" << std::endl;
    
    // 测试多个任务结果
    std::vector<std::pair<std::string, std::string>> tasks = {
        {"task_001", "{\"status\":\"success\",\"output\":\"Task 1 completed\"}"},
        {"task_002", "{\"status\":\"running\",\"output\":\"Task 2 in progress\"}"},
        {"task_003", "{\"status\":\"failed\",\"error\":\"Task 3 failed\"}"}
    };
    
    for (const auto& task : tasks) {
        if (redis.setTaskResult(task.first, task.second, 30)) {
            std::cout << "✓ 设置任务 " << task.first << " 成功" << std::endl;
        } else {
            std::cout << "✗ 设置任务 " << task.first << " 失败" << std::endl;
        }
    }
    
    // 验证所有任务
    for (const auto& task : tasks) {
        std::string result = redis.getTaskResult(task.first);
        if (!result.empty()) {
            std::cout << "✓ 获取任务 " << task.first << " 结果成功" << std::endl;
        } else {
            std::cout << "✗ 获取任务 " << task.first << " 结果失败" << std::endl;
        }
    }
}

void test_expiration(RedisClient& redis) {
    std::cout << "\n=== 测试过期时间 ===" << std::endl;
    
    std::string task_id = "expire_test";
    std::string result = "This will expire in 3 seconds";
    
    if (redis.setTaskResult(task_id, result, 3)) {
        std::cout << "✓ 设置3秒过期任务成功" << std::endl;
        
        // 立即获取
        std::string immediate_result = redis.getTaskResult(task_id);
        if (!immediate_result.empty()) {
            std::cout << "✓ 立即获取结果成功" << std::endl;
        }
        
        // 等待4秒后获取
        std::cout << "等待4秒后重新获取..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(4));
        
        std::string expired_result = redis.getTaskResult(task_id);
        if (expired_result.empty()) {
            std::cout << "✓ 过期后获取失败（预期行为）" << std::endl;
        } else {
            std::cout << "✗ 过期后仍能获取结果（异常）" << std::endl;
        }
    } else {
        std::cout << "✗ 设置过期任务失败" << std::endl;
    }
}

void test_connection_handling(RedisClient& redis) {
    std::cout << "\n=== 测试连接处理 ===" << std::endl;
    
    // 测试错误的主机
    RedisClient test_redis;
    if (!test_redis.connect("invalid_host", 6379, "")) {
        std::cout << "✓ 无效主机连接失败（预期行为）" << std::endl;
    } else {
        std::cout << "✗ 无效主机连接成功（异常）" << std::endl;
    }
    
    // 测试错误的端口
    if (!test_redis.connect("127.0.0.1", 9999, "")) {
        std::cout << "✓ 无效端口连接失败（预期行为）" << std::endl;
    } else {
        std::cout << "✗ 无效端口连接成功（异常）" << std::endl;
    }
}

int main() {
    std::cout << "=== Redis 客户端测试程序 ===" << std::endl;
    
    try {
        RedisClient redis;
        
        // 连接 Redis（请根据你的实际端口和密码修改）
        std::cout << "正在连接 Redis..." << std::endl;
        if (!redis.connect("127.0.0.1", 6379, "")) {
            std::cout << "✗ 连接 Redis 失败，请确保 Redis 服务正在运行" << std::endl;
            std::cout << "提示：可以使用 'redis-server' 启动 Redis 服务" << std::endl;
            return -1;
        }
        std::cout << "✓ 连接 Redis 成功" << std::endl;
        
        // 运行各种测试
        test_basic_operations(redis);
        test_multiple_tasks(redis);
        test_expiration(redis);
        test_connection_handling(redis);
        
        redis.close();
        std::cout << "\n=== 所有测试完成 ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
} 