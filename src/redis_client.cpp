#include "redis_client.h"
#include <cstring>

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

    connected_ = true;
    return true;
}


bool RedisClient::setTaskResult(const std::string &task_id, const std::string &result, int expire_seconds)
{
    // 缓存任务结果
    redisReply* reply = (redisReply*)redisCommand(context_,"SET %s %s",task_id.c_str(),result.c_str());
    bool is_ok = (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0);
    if(reply) freeReplyObject(reply);
    if(!is_ok) return false;

    //设置过期时间
    reply = (redisReply*)redisCommand(context_,"EXPIRE %s %d",task_id.c_str(),expire_seconds);
    is_ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    if(reply) freeReplyObject(reply);
    if(!is_ok) return false;

    return is_ok;
}

std::string RedisClient::getTaskResult(const std::string &task_id)
{
    std::string res;

    // 查询任务结果
    redisReply* reply = (redisReply*)redisCommand(context_,"GET %s",task_id.c_str());
    if(!reply)return res;
    if(reply->type == REDIS_REPLY_STRING)
    {
        res = reply->str;
    }
    if(reply)freeReplyObject(reply);
    return res;
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