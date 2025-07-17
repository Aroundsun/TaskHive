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

int main() {
    task_flow();

    return 0;
} 