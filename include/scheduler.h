#pragma once
#include <string>
#include <memory>
#include "task.pb.h"       // 任务协议
#include "task_queue.h"    // 队列操作
#include "zk_client.h"     // Zookeeper 客户端
#include "message_queue.h" // 消息队列

// 调度器专用队列
class MySchedulerTaskQueue : public TaskQueue<MySchedulerTaskQueue>
{
public:
    MySchedulerTaskQueue(int channel_id = 2) : TaskQueue(channel_id) {}
    bool publishTaskImpl(const taskscheduler::Task &task);
    void mustOverride() const override {}
};
class MySchedulerResultQueue : public ResultQueue<MySchedulerResultQueue>
{
public:
    MySchedulerResultQueue(int channel_id = 3) : ResultQueue(channel_id) {}
    bool consumeResultImpl(taskscheduler::TaskResult &result);
    void mustOverride() const override {}
};
class MySchedulerHeartbeatQueue : public HeartbeatQueue<MySchedulerHeartbeatQueue>
{
public:
    MySchedulerHeartbeatQueue(int channel_id = 4) : HeartbeatQueue(channel_id) {}
    bool consumeHeartbeatImpl(taskscheduler::Heartbeat &hb);
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
    std::string schedulerId_;
    bool running_;
};