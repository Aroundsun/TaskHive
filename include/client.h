#pragma once 

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <thread>
#include<mutex>
#include<atomic>
#include<queue>
#include<condition_variable>
#include "redis_client.h"
#include "task.pb.h"
#include "zk_client.h"

using namespace taskscheduler;




class Client {
public:
    Client();
    ~Client();
    ///////// 内部接口 ///////
    //初始化
    void init();
    //启动
    void start();
    //停止
    void stop();
    //提交任务线程函数
    /*
        主要的职责就是监听任务队列，如果任务队列不为空，
        则取出这个任务并提交给调度器
    */
    void submit_task_threadfunction();
    //监听redis任务执行结果 线程
    /*
        主要职责就是将已经提交给调度器的任务查看是否有结果 
    */
    void consume_task_resultfunction();

    /*
        维护所有调度器节点，定时更新  线程
    */
    void updata_secheduler_node_table_threadfunction();
    /*
        enum 负载均衡方式
        {
            轮询
            cpu 优先
            内存优先
            磁盘优先
            网速优先
            综合优先（实现方式，可以给没一项描述加一个权重，根据权重排）
        }
    */
    //从健康调度器表取出一个健康的调度器节点 -----后续负载均衡的引入位置
    std::pair<std::string,int> get_hearly_secheduler_node();

    //通过socket 提交任务到调度器
    void socket_submit_task_to_scheduler(taskscheduler::Task& task,std::pair<std::string,int>& scheduer_host);



    ///////外部接口/////////
    //提交一个任务
    void submit_one_task(taskscheduler::Task task);
    //提交一组任务
    void submit_more_task(std::vector<taskscheduler::Task> taskarray);
    //提交一个定时任务
    void sunmit_one_timeout(taskscheduler::Task task);

    //根据一个taskid 来查询一个任务结果
    taskscheduler::TaskResult get_task_result(std::string taskid)const;

    //获取当前所有的任务执行结果
    std::vector<taskscheduler::TaskResult> get_all_task_result();

    //查询一个任务结果并移除
    taskscheduler::TaskResult get_task_result_and_delete(std::string taskid);

    //查询当前所有的任务结果并移除
    std::vector<taskscheduler::TaskResult> get_all_task_result_and_delete();



private:
    //客户端停止标识
    std::atomic<bool> running_;
    //Redis 客户端
    std::unique_ptr<RedisClient> redis_client_;
    //zk 客户端
    std::unique_ptr<ZkClient> zk_client_;

    //缓存未分发的任务id
    std::queue<taskscheduler::Task> submit_undistribution_task_;
    std::mutex submit_task_mutex_;
    std::condition_variable submit_task_queue_no_empty_;

    //已经提交给调度器的任务id
    std::vector<std::string> distribution_taskid_;
    std::mutex distribution_taskid_mutex_;

    //缓存获取任务执行失败的id -- 暂不实现
    //std::vector<std::string> exec_all_task_id_;
    //缓存任务执行成功的任务结果
    std::unordered_map<std::string,taskscheduler::TaskResult> taskresult_;
    std::mutex taskresult_mutex_;
    
    struct SchedulerNodeInfo{
        std::string ip;
        int port;
        std::unordered_map<std::string,std::string> descriptor;
        bool undata_flag;
    };
    //缓存所有健康的调度器节点
    std::unordered_map<std::string,SchedulerNodeInfo> hearly_secheduler_node_table_;
    std::mutex hearly_secheduler_node_table_mutex_;

    //提交任务线程
    std::thread submit_task_thread_;
    //获取结果线程
    std::thread get_task_result_thread_;
    //定时更新健康调度器表
    std::thread updata_secheduler_node_table_thread_;
    //处理失败执行任务线程-- 暂不实现


    
};

