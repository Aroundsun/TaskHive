#pragma once
#include <string>
#include <memory>
#include "task.pb.h"       // 任务协议
#include "message_queue.h" // 消息队列
#include "zk_client.h"     // Zookeeper 客户端

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