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
    task.set_task_id("1234567890");
    task.set_type(taskscheduler::TaskType::FUNCTION);
    task.set_content("add_numbers");
    task.mutable_metadata()->insert({"a","1"});
    task.mutable_metadata()->insert({"b","2"});

    std::string task_id = task.task_id();
    //提交任务
    client.submit_one_task(task);

    //等待10秒
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    //获取任务结果
    taskscheduler::TaskResult task_result = client.get_task_result(task_id);
    std::cout << "任务结果: " << task_result.output() << std::endl;
    
    std::cout << "客户端已退出" << std::endl;
    return 0;
}
