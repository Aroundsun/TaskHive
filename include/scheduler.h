#pragma once
#include <string>
#include <memory>
#include "task.pb.h"       // 任务协议
#include "message_queue.h" // 消息队列
#include "zk_client.h"     // Zookeeper 客户端

#define TASK_EXPIRATION_TIME 7200000 // 2小时

// 调度器专用队列
class MySchedulerTaskQueue : public TaskQueue<MySchedulerTaskQueue>
{
public:
    MySchedulerTaskQueue(int channel_id = TASK_CHANNEL_ID) : TaskQueue(channel_id) {}
    bool publishTaskImpl(const taskscheduler::Task &task){
        //检查连接状态
        if(!is_connected_ ||!conn_){
            throw std::runtime_error("SchedulerTaskQueue: 未连接到RabbitMQ");
        }
        //序列化任务
        std::string serialized_task;
        if(!task.SerializeToString(&serialized_task)){
            throw std::runtime_error("SchedulerTaskQueue: 序列化任务失败");
        }
        //构造RabbitMQ需要的参数
        //队列名称
        amqp_bytes_t queueid = amqp_cstring_bytes(queue_name_.c_str());
        //消息内容
        amqp_bytes_t msg_bytes = amqp_cstring_bytes(serialized_task.c_str());
        // 消息属性
        amqp_basic_properties_t props;
        memset(&props, 0, sizeof(props));
        props._flags = AMQP_BASIC_DELIVERY_MODE_FLAG | 
                        AMQP_BASIC_EXPIRATION_FLAG   | 
                        AMQP_BASIC_TYPE_FLAG         | 
                        AMQP_BASIC_TIMESTAMP_FLAG;
        //设置消息持久化
        props.delivery_mode = 2; // 持久化消息 2:持久化消息 1:非持久化消息
        //为持久消息设置过期时间
        props.expiration = amqp_cstring_bytes(std::to_string(TASK_EXPIRATION_TIME).c_str()); // 1小时
        //设置消息类型
        props.type = amqp_cstring_bytes("task");    
        //设置消息创建时间
        props.timestamp = time(nullptr);

        //发布消息
        int ret = amqp_basic_publish(
                            conn_,           
                            channel_id_,
                            amqp_empty_bytes, 
                            queueid, 
                            0, 
                            0, 
                            &props, 
                            msg_bytes);

        //检查发布结果
        if(ret < 0){
            throw std::runtime_error("SchedulerTaskQueue: 发布消息失败");
        }

        return true;
    }
    bool consumeTaskImpl(TaskCallback cb) { throw std::runtime_error("Not implemented"); }
    void mustOverride() const override {}
};
class MySchedulerResultQueue : public ResultQueue<MySchedulerResultQueue>
{
public:
    MySchedulerResultQueue(int channel_id = RESULT_CHANNEL_ID) : ResultQueue(channel_id) {}
    bool publishResultImpl(const taskscheduler::TaskResult&) { throw std::runtime_error("Not implemented"); }
    bool consumeResultImpl(ResultCallback result_cb)
    {
        //检查连接状态
        if(!is_connected_ || !conn_)
        {
            throw std::runtime_error("consumeResultImpl 未连接到RabbitMQ");
        }
        //声明队列
        //略 消息队列基类connect 函数中已经声明过
        //注册消费者
        amqp_basic_consume(conn_,
                        channel_id_,
                        amqp_cstring_bytes(queue_name_.c_str()),
                        amqp_empty_bytes,
                        0,
                        1,
                        0,
                        amqp_empty_table
                        );
        //监听消费结果
        while(is_connected_){
            /*
            当成功消费完一条消息后，这些数据其实还在 buffer 里，没有立即释放；
            所以你需要调用 amqp_maybe_release_buffers(conn)，来尝试释放这些已经处理完的网络数据，节省内存。
            */
            amqp_maybe_release_buffers(conn_);
            //定义信封
            amqp_envelope_t envelope;
            //消费消息 -阻塞等待消息
            amqp_rpc_reply_t ret = amqp_consume_message(conn_, &envelope, nullptr, 0);
            if(ret.reply_type == AMQP_RESPONSE_NORMAL){
                //解析消息
                taskscheduler::TaskResult result;
                std::string msg_body(static_cast<char*>(envelope.message.body.bytes), envelope.message.body.len);
                if(!result.ParseFromString(msg_body)){
                    throw std::runtime_error("SchedulerResultQueue: 解析消息失败");
                }
                // 处理消息
                try{
                    result_cb(result);//对消息的处理由使用者实现
                }
                catch(const std::exception& e){
                    std::cerr << "SchedulerResultQueue: 处理消息失败" << e.what() << std::endl;
                }
                // 确认消息
                amqp_basic_ack(conn_, channel_id_, envelope.delivery_tag, 0);
                //销毁envelope
                amqp_destroy_envelope(&envelope);
            }
            else {
                throw std::runtime_error("SchedulerResultQueue: 消费消息失败");
            }
        }

        return true;
    }

    void mustOverride() const override {}
};
class MySchedulerHeartbeatQueue : public HeartbeatQueue<MySchedulerHeartbeatQueue>
{
public:
    MySchedulerHeartbeatQueue(int channel_id = HEARTBEAT_CHANNEL_ID) : HeartbeatQueue(channel_id) {}
    bool publishHeartbeatImpl(const taskscheduler::Heartbeat&) { throw std::runtime_error("Not implemented"); }
    bool consumeHeartbeatImpl(HeartbeatCallback hb_cb)
    {
        //检查连接状态
        if(!is_connected_ || !conn_)
        {
            throw std::runtime_error("consumeHeartbeatImpl 未连接到RabbitMQ");
        }
        //声明队列
        //略 消息队列基类connect 函数中已经声明过
        //注册消费者
        amqp_basic_consume(conn_,
                        channel_id_,
                        amqp_cstring_bytes(queue_name_.c_str()),
                        amqp_empty_bytes,
                        0,
                        1,
                        0,
                        amqp_empty_table
                        );
        //监听消费结果
        while(is_connected_){
            /*
            当成功消费完一条消息后，这些数据其实还在 buffer 里，没有立即释放；
            所以你需要调用 amqp_maybe_release_buffers(conn)，来尝试释放这些已经处理完的网络数据，节省内存。
            */
            amqp_maybe_release_buffers(conn_);
            //定义信封
            amqp_envelope_t envelope;
            //消费消息 -阻塞等待消息
            amqp_rpc_reply_t ret = amqp_consume_message(conn_, &envelope, nullptr, 0);
            if(ret.reply_type == AMQP_RESPONSE_NORMAL){
                //解析消息
                taskscheduler::Heartbeat heartbeat;
                std::string msg_body(static_cast<char*>(envelope.message.body.bytes), envelope.message.body.len);
                if(!heartbeat.ParseFromString(msg_body)){
                    throw std::runtime_error("SchedulerHeartbeatQueue: 解析消息失败");
                }
                try{
                    // 处理消息
                    hb_cb(heartbeat);//对消息的处理由使用者实现
                }
                catch(const std::exception& e){
                    std::cerr << "SchedulerHeartbeatQueue: 处理消息失败" << e.what() << std::endl;
                }
                // 确认消息
                amqp_basic_ack(conn_, channel_id_, envelope.delivery_tag, 0);
                //销毁envelope
                amqp_destroy_envelope(&envelope);
            }   
            else {
                throw std::runtime_error("SchedulerHeartbeatQueue: 消费消息失败");
            }
        }   

        return true;
    }
    void mustOverride() const override {}
};

class Scheduler
{
public:
    Scheduler();
    ~Scheduler();

    // 初始化调度器（连接ZK、RabbitMQ等）
    bool init(const std::string &zk_addr, const std::string &mq_host, int mq_port,
              const std::string &mq_user, const std::string &mq_pass);

    // 启动调度器主循环
    void start();

    // 停止调度器
    void stop();

    // 接收任务（可由客户端/接口调用）
    void submitTask(const taskscheduler::Task &task);

    // 处理任务结果
    void handleTaskResult(const taskscheduler::TaskResult &result);

    // 负载均衡分发任务到 worker
    void dispatchTask(const taskscheduler::Task &task);

    // 节点注册到 Zookeeper
    void registerToZk();

    // 心跳上报
    void sendHeartbeat();

    // 其他辅助接口
    // ...

private:
    std::unique_ptr<ZkClient> zkClient_;

    std::shared_ptr<MySchedulerTaskQueue> taskQueue_;
    std::shared_ptr<MySchedulerResultQueue> resultQueue_;
    std::shared_ptr<MySchedulerHeartbeatQueue> heartbeatQueue_;


    std::string schedulerId_;
    bool running_;
};