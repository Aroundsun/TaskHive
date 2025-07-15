#pragma once
#include "message_queue.h"
#include "task.pb.h"
#include "redis_client.h"
#include "zk_client.h"

// 工作器专用队列
class MyWorkerTaskQueue : public TaskQueue<MyWorkerTaskQueue>
{
public:
    MyWorkerTaskQueue(int channel_id = 2) : TaskQueue(channel_id) {}
    bool consumeTaskImpl(taskscheduler::Task &task);
    void mustOverride() const override {}
};
class MyWorkerResultQueue : public ResultQueue<MyWorkerResultQueue>
{
public:
    MyWorkerResultQueue(int channel_id = 3) : ResultQueue(channel_id) {}
    bool publishResultImpl(const taskscheduler::TaskResult &result);
    void mustOverride() const override {}
};
class MyWorkerHeartbeatQueue : public HeartbeatQueue<MyWorkerHeartbeatQueue>
{
public:
    MyWorkerHeartbeatQueue(int channel_id = 4) : HeartbeatQueue(channel_id) {}
    bool publishHeartbeatImpl(const taskscheduler::Heartbeat &hb);
    void mustOverride() const override {}
};