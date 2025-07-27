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
#include<scheduler_config.h>
#include<unordered_map>
#include<mutex>

// 系统信息结构
struct SystemInfo {
    double cpu_usage = 0.0;
    double memory_usage = 0.0;
    double disk_usage = 0.0;
    double network_speed = 0.0;  // 网络速度，单位Mbps
    std::string timestamp;
};

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
    void receive_task_thread_function();

    //提交任务
    void submit_task_thread_function();

    //获取任务结果
    void get_task_result_thread_function();

    //上报心跳
    void report_heartbeat_thread_function();

    //上报健康心跳
    void report_healthy_heartbeat_to_zk();

    //上报不健康心跳
    void report_unhealthy_heartbeat_to_zk();

    //系统信息维护线程
    void system_info_maintenance_thread_function();

    //获取系统信息
    SystemInfo get_system_info() const;
    //更新系统信息
    void update_system_info();
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

    //监听套接字
    int listen_socket_fd_ = -1;

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

    //系统信息维护线程
    std::thread system_info_maintenance_thread_;

    //系统信息存储
    mutable std::mutex system_info_mutex_;
    SystemInfo current_system_info_;



};