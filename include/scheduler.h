#pragma once
#include <string>
#include <memory>
#include<atomic>
#include "task.pb.h"       // 任务协议
#include "message_queue.h" // 消息队列
#include "zk_client.h"     // Zookeeper 客户端
#include <thread>
#include "redis_client.h"
#include<queue>
#include<condition_variable>
//调度器
class scheduler{

public:
    scheduler();
    ~scheduler();

    //启动调度器
    void start();

    //初始化
    void init();

    //停止调度器
    void stop();

    //接收任务
    void receive_task();

    //提交任务
    void submit_task();

    //获取任务结果
    void get_task_result();

    //上报心跳
    void report_heartbeat();




private:
    //运行状态
    std::atomic<bool> running_;

    //zk客户端
    std::shared_ptr<ZkClient> zkcli_;
    //redis客户端
    std::shared_ptr<RedisClient> rediscli_;

    //调度器侧任务队列
    std::unique_ptr<MySchedulerTaskQueue> scheduler_task_queue_;
    //调度器侧任务结果队列
    std::unique_ptr<MySchedulerResultQueue> scheduler_task_result_queue_;

    //待分发的任务队列
    std::queue<taskscheduler::Task> pending_tasks_;
    //待分发的任务队列锁
    std::mutex pending_tasks_mutex_;

    //条件变量，用来等待待提交的任务队列有任务
    std::condition_variable pending_tasks_queue_not_empty_;
    
    //接收任务线程
    std::thread receive_task_thread_;
    //提交任务线程
    std::thread submit_task_thread_;
    //获取任务结果线程
    std::thread get_task_result_thread_;

    //上报心跳线程
    std::thread report_heartbeat_thread_;

    

};