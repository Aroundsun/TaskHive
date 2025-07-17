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

        redis.close();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
} 