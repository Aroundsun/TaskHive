#include "client.h"
#include <iostream>
#include "task.pb.h"
int main() {
    std::cout << "TaskHive 客户端启动..." << std::endl;
    
    // 创建客户端实例
    Client client;
    
    // 启动客户端
    client.start();
    
    //创建以一个任务

    taskscheduler::Task task;
    task.set_task_id("task-1");
    task.set_type(taskscheduler::TaskType::FUNCTION);
    task.set_content("add_numbers");
    task.mutable_metadata()->insert({"a","9"});
    task.mutable_metadata()->insert({"b","990"});

    std::string task_id = task.task_id();
    //提交任务
    client.submit_one_task(task);


    //阻塞等待任务结果 等10秒
    std::this_thread::sleep_for(std::chrono::seconds(3));
        //获取任务结果
    taskscheduler::TaskResult task_result = client.get_task_result(task_id);
    std::cout << "任务结果: " << task_result.output() << std::endl;

    //////////////////
    //创建以一个任务
    taskscheduler::Task task1;
    task1.set_task_id("task-2");
    task1.set_type(taskscheduler::TaskType::FUNCTION);
    task1.set_content("add_numbers");
    task1.mutable_metadata()->insert({"a","100"});
    task1.mutable_metadata()->insert({"b","11"});

    std::string task_id1 = task1.task_id();
    //提交任务
    client.submit_one_task(task1);

    //阻塞等待任务结果 等10秒
    std::this_thread::sleep_for(std::chrono::seconds(3));
        //获取任务结果
    taskscheduler::TaskResult task_result1 = client.get_task_result(task_id1);
    std::cout << "任务结果: " << task_result1.output() << std::endl;

    return 0;
}
