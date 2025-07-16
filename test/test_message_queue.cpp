#include "scheduler.h"
#include "worker.h"
#include "task.pb.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>

// 任务链路：任务发布 -> 任务消费 -> 结果发布 -> 结果消费
void task_flow() {
    std::cout << "===== 任务链路测试 =====" << std::endl;
    auto schedulerTaskQueue = std::make_shared<MySchedulerTaskQueue>();
    auto workerTaskQueue = std::make_shared<MyWorkerTaskQueue>();
    auto workerResultQueue = std::make_shared<MyWorkerResultQueue>();
    auto schedulerResultQueue = std::make_shared<MySchedulerResultQueue>();
    // 连接
    schedulerTaskQueue->connect("localhost", 5672, "guest", "guest");
    workerTaskQueue->connect("localhost", 5672, "guest", "guest");
    workerResultQueue->connect("localhost", 5672, "guest", "guest");
    schedulerResultQueue->connect("localhost", 5672, "guest", "guest");

    // 1. 发布任务（调度器）
    taskscheduler::Task task;
    task.set_task_id("sched_t1");
    std::thread t1([schedulerTaskQueue, task](){
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        schedulerTaskQueue->publishTask(task);
        std::cout << "[调度器] 已发布任务: " << task.task_id() << std::endl;
    });

    // 2. Worker 消费任务并发布结果
    std::thread t2([workerTaskQueue, workerResultQueue](){
        try {
            workerTaskQueue->consumeTask([workerResultQueue](taskscheduler::Task &t){
                std::cout << "[Worker] 已消费任务: " << t.task_id() << std::endl;
                // 模拟处理后发布结果
                taskscheduler::TaskResult result;
                result.set_task_id(t.task_id());
                result.set_status(taskscheduler::TaskStatus::SUCCESS);
                workerResultQueue->publishResult(result);
                std::cout << "[Worker] 已发布结果: " << result.task_id() << std::endl;
            });
        } catch (const std::exception& e) {
            std::cout << "[Worker] 任务消费线程退出: " << e.what() << std::endl;
        }
    });

    // 3. 调度器消费结果
    std::thread t3([schedulerResultQueue](/*捕获所有队列防止悬垂*/){
        try {
            schedulerResultQueue->consumeResult([](taskscheduler::TaskResult &r){
                std::cout << "[调度器] 已消费结果: " << r.task_id() << " 状态: " << r.status() << std::endl;
            });
        } catch (const std::exception& e) {
            std::cout << "[调度器] 结果消费线程退出: " << e.what() << std::endl;
        }
    });

    t1.join();
    t2.join();
    t3.join();
    // 由主线程统一关闭
    schedulerTaskQueue->close();
    workerTaskQueue->close();
    workerResultQueue->close();
    schedulerResultQueue->close();
}

// 心跳链路：心跳发布 -> 心跳消费
void heartbeat_flow() {
    std::cout << "===== 心跳链路测试 =====" << std::endl;
    auto workerHeartbeatQueue = std::make_shared<MyWorkerHeartbeatQueue>();
    auto schedulerHeartbeatQueue = std::make_shared<MySchedulerHeartbeatQueue>();
    workerHeartbeatQueue->connect("localhost", 5672, "guest", "guest");
    schedulerHeartbeatQueue->connect("localhost", 5672, "guest", "guest");

    // 用于同步心跳消费完成
    std::atomic<bool> heartbeat_consumed{false};

    // 发布心跳
    std::thread t1([workerHeartbeatQueue](){
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        taskscheduler::Heartbeat hb;
        hb.set_worker_id("worker-1");
        hb.set_timestamp(time(nullptr));
        workerHeartbeatQueue->publishHeartbeat(hb);
        std::cout << "[Worker] 已发布心跳: " << hb.worker_id() << std::endl;
    });
    // 消费心跳
    std::thread t2([schedulerHeartbeatQueue, &heartbeat_consumed](){
        try {
            schedulerHeartbeatQueue->consumeHeartbeat([&](taskscheduler::Heartbeat &h){
                std::cout << "[调度器] 已消费心跳: " << h.worker_id() << std::endl;
                heartbeat_consumed = true;
            });
        } catch (const std::exception& e) {
            std::cout << "[调度器] 心跳消费线程退出: " << e.what() << std::endl;
        }
    });
    t1.join();
    // 等待心跳被消费
    while (!heartbeat_consumed) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    t2.detach(); // 消费线程已完成任务，主线程关闭队列
    workerHeartbeatQueue->close();
    schedulerHeartbeatQueue->close();
}

int main() {
    task_flow();
    heartbeat_flow();
    return 0;
} 