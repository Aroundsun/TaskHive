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

    // 缓存任务结果 (Database 0)
    bool setTaskResult(const std::string& task_id, const std::string& result, int expire_seconds = 3600);
    std::string getTaskResult(const std::string& task_id);

    // 缓存/更新 Worker 心跳信息 (Database 1)
    bool setWorkerHeartbeat(const std::string& worker_id, const std::unordered_map<std::string, std::string>& info, int expire_seconds = 60);
    std::unordered_map<std::string, std::string> getWorkerHeartbeat(const std::string& worker_id);

    // 关闭连接
    void close();

private:
    redisContext* context_;
    bool connected_;
    
    // 选择数据库
    bool selectDatabase(int db);
}; 