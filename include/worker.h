#pragma once
#include "message_queue.h"
#include "task.pb.h"
#include "redis_client.h"
#include "zk_client.h"
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>





//执行期工作器
class Worker
{
public:
    Worker();
    ~Worker();

    //初始化
    void init();
    //启动
    void start();
    //停止
    void stop();
    //接收任务
    void receive_task();
    //上报任务结果
    void report_task_result();
    //上报心跳
    void report_heartbeat();
    //执行任务
    void exec_task();

private:
    
    std::atomic<bool> running_;//运行状态

    
    std::shared_ptr<ZkClient> zkcli_;//zk客户端
    
    std::queue<taskscheduler::Task> pending_tasks_;//待执行任务缓存队列
    std::mutex pending_tasks_mutex_;//待执行任务缓存队列互斥锁
    std::condition_variable pending_tasks_queue_not_empty_;//待执行任务缓存队列条件变量

    std::queue<taskscheduler::TaskResult> task_result_queue_;//任务结果缓存队列
    std::mutex task_result_queue_mutex_;//任务结果缓存队列互斥锁
    std::condition_variable task_result_queue_not_empty_;//任务结果缓存队列条件变量

    
    size_t pending_tasks_queue_size_;//待执行任务缓存队列大小
    std::mutex pending_tasks_queue_size_mutex_;//待执行任务缓存队列大小互斥锁

    
    std::unique_ptr<MyWorkerTaskQueue> worker_task_queue_;//工作端任务队列
    std::unique_ptr<MyWorkerResultQueue> worker_result_queue_;//工作端任务结果队列

    
    std::thread receive_task_thread_;//消费任务线程
    std::thread consume_task_result_thread_;//消费任务结果线程
    std::thread report_heartbeat_thread_;//上报心跳线程
    std::thread exec_task_thread_;//执行任务线程


};