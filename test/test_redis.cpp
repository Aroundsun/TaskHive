#include "redis_client.h"
#include <iostream>
#include <unordered_map>

int main() {
    try {
        RedisClient redis;
        // 连接 Redis（请根据你的实际端口和密码修改）
        redis.connect("127.0.0.1", 6379, "");

        // 测试 setTaskResult/getTaskResult
        std::string task_id = "test_task_001";
        std::string result = "success";
        if (redis.setTaskResult(task_id, result, 60)) {
            std::cout << "setTaskResult OK" << std::endl;
        } else {
            std::cout << "setTaskResult FAIL" << std::endl;
        }

        std::string get_result = redis.getTaskResult(task_id);
        std::cout << "getTaskResult: " << get_result << std::endl;

        // 测试 setWorkerHeartbeat/getWorkerHeartbeat
        std::string worker_id = "worker_001";
        std::unordered_map<std::string, std::string> info = {
            {"ip", "192.168.1.100"},
            {"status", "online"},
            {"timestamp", "1721000000"}
        };
        if (redis.setWorkerHeartbeat(worker_id, info, 30)) {
            std::cout << "setWorkerHeartbeat OK" << std::endl;
        } else {
            std::cout << "setWorkerHeartbeat FAIL" << std::endl;
        }

        auto get_info = redis.getWorkerHeartbeat(worker_id);
        std::cout << "getWorkerHeartbeat:" << std::endl;
        for (const auto& kv : get_info) {
            std::cout << "  " << kv.first << ": " << kv.second << std::endl;
        }

        redis.close();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
} 