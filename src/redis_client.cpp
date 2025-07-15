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

bool RedisClient::selectDatabase(int db)
{
    if (!connected_)
        return false;
    redisReply *reply = (redisReply *)redisCommand(context_, "SELECT %d", db);
    if (!reply )
        return false;
    bool result = (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0);
    freeReplyObject(reply);
    return result;
}

bool RedisClient::setTaskResult(const std::string &task_id, const std::string &result, int expire_seconds)
{
    // 选择 Database 0 存储任务结果
    if (!selectDatabase(0))
        return false;
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
    // 选择 Database 0 查询任务结果
    if (!selectDatabase(0))
        return res;
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

bool RedisClient::setWorkerHeartbeat(const std::string &worker_id, const std::unordered_map<std::string, std::string> &info, int expire_seconds)
{
    // 选择 Database 1 存储心跳信息
    if (!selectDatabase(1))
        return false;

    // 缓存/更新 Worker 心跳信息
    //构造 HMSET 
    std::string cmd = "HMSET "+worker_id;
    for(auto &kv : info)
    {
        cmd += " " + kv.first + " " + kv.second;
    }
    //执行HMSET
    redisReply* reply = (redisReply*)redisCommand(context_,cmd.c_str());
    bool is_ok = (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0);
    if(reply) freeReplyObject(reply);
    
    //设置过期时间
    reply = (redisReply*)redisCommand(context_,"EXPIRE %s %d",worker_id.c_str(),expire_seconds);
    is_ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    if(reply) freeReplyObject(reply);
    
    return is_ok;
}

std::unordered_map<std::string, std::string> RedisClient::getWorkerHeartbeat(const std::string &worker_id)
{
    std::unordered_map<std::string,std::string> resMap;

    // 选择 Database 1 查询心跳信息
    if (!selectDatabase(1))
        return resMap;
    // 查询 Worker 心跳信息
    redisReply* reply = (redisReply*)redisCommand(context_,"HGETALL %s",worker_id.c_str());
    if(!reply)return resMap;
    //解析心跳信息 并存入resMap
    if(reply->type == REDIS_REPLY_ARRAY)
    {
        for(size_t i = 0; i+1 <reply->elements;i+=2)
        {
            redisReply*field = reply->element[i];
            redisReply*value = reply->element[i+1];
            if(field->type == REDIS_REPLY_STRING && value ->type == REDIS_REPLY_STRING)
            {
                resMap[field->str] = value->str;
            }
        }
    }
    if(reply) freeReplyObject(reply);
    return resMap;
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