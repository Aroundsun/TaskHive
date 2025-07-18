#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <hiredis/hiredis.h>  
#include <stdexcept> 

class RedisClient {
public:
    RedisClient();
    ~RedisClient();

    // 连接 Redis
    bool connect(const std::string& host, int port, const std::string& password = "");

    // 缓存任务结果
    bool setTaskResult(const std::string& task_id, const std::string& result, int expire_seconds = 3600);
    
    std::string getTaskResult(const std::string& task_id);

    

    // 关闭连接
    void close();

private:
    redisContext* context_;
    bool connected_;
}; 