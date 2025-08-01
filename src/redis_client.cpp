#include "redis_client.h"
#include <cstring>
#include <iostream>
#include <ostream>
RedisClient::RedisClient() : context_(nullptr), connected_(false) {}

RedisClient::~RedisClient()
{
    close();
}

bool RedisClient::connect(const std::string &host, int port, const std::string &password)
{
    // 连接 Redis 服务器
    context_ = redisConnect(host.c_str(), port);

    // 检查连接是否成功
    if (!context_)
    {
        throw std::runtime_error("Failed to allocate redis context");
        return false;
    }

    if (context_->err)
    {
        std::string error_msg = "Redis connection failed: " + std::string(context_->errstr);
        redisFree(context_);
        context_ = nullptr;
        throw std::runtime_error(error_msg);
        return false;
    }

    // 如果设置了密码，进行认证
    if (!password.empty())
    {
        redisReply *reply = (redisReply *)redisCommand(context_, "AUTH %s", password.c_str());
        if (reply == nullptr)
        {
            redisFree(context_);
            context_ = nullptr;
            throw std::runtime_error("Failed to authenticate with Redis");
            return false;
        }

        if (reply->type == REDIS_REPLY_ERROR)
        {
            std::string error_msg = "Redis authentication failed: " + std::string(reply->str);
            freeReplyObject(reply);
            redisFree(context_);
            context_ = nullptr;
            throw std::runtime_error(error_msg);
            return false;
        }
        freeReplyObject(reply);
    }
    std::cout << "redis 连接成功！！！！！！！！！！！" << std::endl;

    connected_ = true;
    return true;
}


bool RedisClient::setTaskResult(const std::string &task_id, const taskscheduler::TaskResult &result, int expire_seconds)
{
    // 使用哈希表存储任务结果
    std::string key = task_id;
    //将result序列化
    std::string result_data;
    result.SerializeToString(&result_data);
    
    std::cout << "[Redis] 存储任务结果: " << task_id << ", 数据大小: " << result_data.size() << std::endl;
    
    redisReply* reply = (redisReply*)redisCommand(context_,"HSET %s result %s",key.c_str(),result_data.c_str());
    bool is_ok = (reply && (reply->type == REDIS_REPLY_INTEGER || reply->type == REDIS_REPLY_STATUS));
    if(reply) freeReplyObject(reply);

    if(!is_ok) {
        std::cerr << "[Redis] 存储任务结果失败: " << task_id << std::endl;
        return false;
    }

    //设置过期时间
    reply = (redisReply*)redisCommand(context_,"EXPIRE %s %d",key.c_str(),expire_seconds);
    is_ok = (reply && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    if(reply) freeReplyObject(reply);
    if(!is_ok) {
        std::cerr << "[Redis] 设置过期时间失败: " << task_id << std::endl;
        return false;
    }

    std::cout << "[Redis] 任务结果存储成功: " << task_id << std::endl;
    return is_ok;
}

std::string RedisClient::getTaskResult(const std::string &task_id)
{
    std::string res = "NO_RESULT";
    // 从哈希表查询任务结果
    std::string key = task_id;
    redisReply* reply = (redisReply*)redisCommand(context_,"HGET %s result",key.c_str());
    if(!reply) {
        std::cerr << "[Redis] 查询任务结果失败，reply为空: " << task_id << std::endl;
        return res;
    }
    if(reply->type == REDIS_REPLY_STRING)
    {        
        res = std::string(reply->str, reply->len);
        std::cout << "[Redis] 查询到任务结果: " << task_id << ", 数据大小: " << res.size() << std::endl;
    }
    else if(reply->type == REDIS_REPLY_NIL) {
        std::cout << "[Redis] 任务结果不存在: " << task_id << std::endl;
    }
    else {
        std::cerr << "[Redis] 查询任务结果类型错误: " << task_id << ", 类型: " << reply->type << std::endl;
    }
    if(reply) freeReplyObject(reply);
    return res;
}

bool RedisClient::deleteTaskResult(const std::string& task_id)
{
    std::string key = task_id;
    redisReply* reply = (redisReply*)redisCommand(context_,"HDEL %s result",key.c_str());
    bool is_ok = (reply && (reply->type == REDIS_REPLY_INTEGER || reply->type == REDIS_REPLY_STATUS));
    if(reply) freeReplyObject(reply);   
    return is_ok;
}


void RedisClient::close()
{
    // 关闭连接
    if (context_)
    {
        redisFree(context_);
        context_ = nullptr;
        connected_ = false;
    }
}