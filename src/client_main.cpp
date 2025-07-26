#include "client.h"
#include <iostream>
#include "task.pb.h"
#include <set>
#include <thread>
#include <chrono>
int main() {
    std::cout << "TaskHive 客户端启动..." << std::endl;
    
    // 创建客户端实例
    Client client;
    
    // 启动客户端
    client.start();
    
    // 提交100个任务
    std::cout << "开始提交任务..." << std::endl;
    for(int i = 0;i<100;++i){
        taskscheduler::Task task;
        task.set_task_id("task-"+std::to_string(i+1));
        task.set_type(taskscheduler::TaskType::FUNCTION);
        task.set_content("add_numbers");
        task.mutable_metadata()->insert({"a",std::to_string(i)});
        task.mutable_metadata()->insert({"b",std::to_string(i)});
        client.submit_one_task(task);
    }
    std::cout << "任务提交完成，等待结果..." << std::endl;
    
    // 等待一段时间让任务执行
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // 轮询查询任务结果
    int max_attempts = 30; // 最多等待30秒
    int attempts = 0;
    std::set<std::string> completed_tasks;
    
    while(completed_tasks.size() < 100 && attempts < max_attempts) {
        for(int i = 0;i<100;++i) {
            std::string task_id = "task-"+std::to_string(i+1);
            if(completed_tasks.find(task_id) == completed_tasks.end()) {
                taskscheduler::TaskResult task_result = client.get_task_result(task_id);
                if(task_result.task_id() != "") { // 检查是否获取到有效结果
                    std::cout << "任务 " << task_id << " 结果: " << task_result.output() 
                              << ", 状态: " << task_result.status() << std::endl;
                    completed_tasks.insert(task_id);
                }
            }
        }
        
        if(completed_tasks.size() < 100) {
            std::cout << "已完成的任务: " << completed_tasks.size() << "/100, 继续等待..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            attempts++;
        }
    }
    
    if(completed_tasks.size() < 100) {
        std::cout << "警告：部分任务未完成，已完成: " << completed_tasks.size() << "/100" << std::endl;
    } else {
        std::cout << "所有任务已完成！" << std::endl;
    }

    // taskscheduler::Task task;
    // task.set_task_id("task-1");
    // task.set_type(taskscheduler::TaskType::COMMAND);
    // task.set_content("ls");
    // task.mutable_metadata()->insert({"-","la"});
    // //task.mutable_metadata()->insert({"b","990"});

    // std::string task_id = task.task_id();
    // //提交任务
    // client.submit_one_task(task);


    // //阻塞等待任务结果 等10秒
    // std::this_thread::sleep_for(std::chrono::seconds(3));
    //     //获取任务结果
    // taskscheduler::TaskResult task_result = client.get_task_result(task_id);
    // std::cout << "任务结果: " << task_result.output() << std::endl;

    // //////////////////
    // //创建以一个任务
    // taskscheduler::Task task1;
    // task1.set_task_id("task-2");
    // task1.set_type(taskscheduler::TaskType::FUNCTION);
    // task1.set_content("add_numbers");
    // task1.mutable_metadata()->insert({"a","100"});
    // task1.mutable_metadata()->insert({"b","11"});

    // std::string task_id1 = task1.task_id();
    // //提交任务
    // client.submit_one_task(task1);


    // //阻塞等待任务结果 等10秒
    // std::this_thread::sleep_for(std::chrono::seconds(3));
    //     //获取任务结果
    // taskscheduler::TaskResult task_result1 = client.get_task_result(task_id1);
    // std::cout << "任务结果: " << task_result1.output() << std::endl;

    
   

    return 0;
}
