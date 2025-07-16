#pragma once
#include <string>
#include <stdexcept>
#include "task.pb.h"
extern "C"
{
#include <amqp.h>
#include <amqp_tcp_socket.h>
}

// channel_id 常量集中管理
#define TASK_CHANNEL_ID 2
#define RESULT_CHANNEL_ID 3
#define HEARTBEAT_CHANNEL_ID 4

// 消息队列基类
class MessageQueue
{
public:

    explicit MessageQueue(int channel_id) : channel_id_(channel_id) {
        if (channel_id != TASK_CHANNEL_ID && channel_id != RESULT_CHANNEL_ID && channel_id != HEARTBEAT_CHANNEL_ID) {
            throw std::invalid_argument("MessageQueue: 非法的 channel_id");
        }
        conn_ = nullptr;
        socket_ = nullptr;
        is_connected_ = false;
    }
    virtual ~MessageQueue() {}
    
    virtual bool connect(const std::string &mq_host, int mq_port,
                         const std::string &mq_user, const std::string &mq_pass)
    {
        //保存连接信息
        mq_host_ = mq_host;
        mq_port_ = mq_port;
        mq_user_ = mq_user;
        mq_pass_ = mq_pass;
        //创建连接
        conn_ = amqp_new_connection();
        if(!conn_){
            throw std::runtime_error("MessageQueue: 创建连接失败");
            return false;
        }
        //创建socket
        amqp_socket_t* socket = amqp_tcp_socket_new(conn_);
        if(!socket)
        {
            throw std::runtime_error("MessageQueue: 创建socket失败");
            return false;
        }
        //连接RabbitMQ 
        if(amqp_socket_open(socket,mq_host_.c_str(),mq_port) != AMQP_STATUS_OK)
        {
            throw std::runtime_error("MessageQueue: 连接RabbitMQ 失败");
            return false;
        }
        //登陆RabbitMQ
        amqp_rpc_reply_t login_reply = amqp_login(conn_,
                                                "/",
                                                0,
                                                128*1024,
                                                0,
                                                AMQP_SASL_METHOD_PLAIN,
                                                mq_user_.c_str(),
                                                mq_pass_.c_str()
        );
        if(login_reply.reply_type != AMQP_RESPONSE_NORMAL)
        {
            throw std::runtime_error("MessageQueue: 登陆RabbitMQ 失败");
            return false;
        }
        //打开channel
        amqp_channel_open(conn_,channel_id_);
        amqp_rpc_reply_t channel_reply = amqp_get_rpc_reply(conn_);
        if(channel_reply.reply_type != AMQP_RESPONSE_NORMAL)
        {
            throw std::runtime_error("MessageQueue: 打开channel失败");
            return false;
        }
        //声明队列
        amqp_bytes_t queueid = amqp_cstring_bytes(queue_name_.c_str());
        amqp_queue_declare(conn_, channel_id_, queueid, 0, 1, 0, 0, amqp_empty_table);
        //标记连接成功
        is_connected_ = true;
        return true;
    }
    
    virtual void close() {
        if (is_connected_ && conn_) {
            // 关闭 channel
            amqp_channel_close(conn_, channel_id_, AMQP_REPLY_SUCCESS);
            
            // 关闭连接
            amqp_connection_close(conn_, AMQP_REPLY_SUCCESS);
            
            // 销毁连接
            amqp_destroy_connection(conn_);
            
            // 重置状态
            conn_ = nullptr;
            socket_ = nullptr;
            is_connected_ = false;
        }
    }
    int getChannelId() const { return channel_id_; }
protected:
    std::string queue_name_;
    std::string mq_host_;
    int mq_port_ = 0;
    std::string mq_user_;
    std::string mq_pass_;
    amqp_connection_state_t conn_ ;
    amqp_socket_t *socket_ ;
    bool is_connected_;
    int channel_id_;
};

// 任务队列基类
template <typename Derived>
class TaskQueue : public MessageQueue
{
public:
// 任务队列消费回调
using TaskCallback = std::function<void(taskscheduler::Task &task)>;

protected:
    TaskQueue(int channel_id = TASK_CHANNEL_ID) : MessageQueue(channel_id) {
        if (channel_id != TASK_CHANNEL_ID) {
            throw std::invalid_argument("TaskQueue: 非法的 channel_id");
        }
        queue_name_ = "task_queue";
    }
    // 防止直接实例化
    virtual void mustOverride() const = 0;
public:
    virtual bool publishTask(const taskscheduler::Task &task)
    {
        return static_cast<Derived *>(this)->publishTaskImpl(task);
    }
    virtual bool consumeTask(TaskCallback callback)
    {
        return static_cast<Derived *>(this)->consumeTaskImpl(callback);
    }
};

// 结果队列基类
template <typename Derived>
class ResultQueue : public MessageQueue
{
public:
// 结果队列消费回调
using ResultCallback = std::function<void(taskscheduler::TaskResult &result)>;

protected:
    ResultQueue(int channel_id = RESULT_CHANNEL_ID) : MessageQueue(channel_id) {
        if (channel_id != RESULT_CHANNEL_ID) {
            throw std::invalid_argument("ResultQueue: 非法的 channel_id");
        }
        queue_name_ = "result_queue";
    }
    // 防止直接实例化
    virtual void mustOverride() const = 0;
public:
    virtual bool publishResult(const taskscheduler::TaskResult &result)
    {
        return static_cast<Derived *>(this)->publishResultImpl(result);
    }
    virtual bool consumeResult(ResultCallback callback) 
    {
        return static_cast<Derived *>(this)->consumeResultImpl(callback);
    }
};

// 心跳队列基类
template <typename Derived>
class HeartbeatQueue : public MessageQueue
{
public:
// 心跳队列消费回调
using HeartbeatCallback = std::function<void(taskscheduler::Heartbeat &hb)>;

protected:
    HeartbeatQueue(int channel_id = HEARTBEAT_CHANNEL_ID) : MessageQueue(channel_id) {
        if (channel_id != HEARTBEAT_CHANNEL_ID) {
            throw std::invalid_argument("HeartbeatQueue: 非法的 channel_id");
        }
        queue_name_ = "heartbeat_queue";
    }
    // 防止直接实例化
    virtual void mustOverride() const = 0;
public:
    virtual bool publishHeartbeat(const taskscheduler::Heartbeat &hb)
    {
        return static_cast<Derived *>(this)->publishHeartbeatImpl(hb);
    }
    virtual bool consumeHeartbeat(HeartbeatCallback callback)
    {
        return static_cast<Derived *>(this)->consumeHeartbeatImpl(callback);
    }
};
