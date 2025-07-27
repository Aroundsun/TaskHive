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
#include<algorithm>
#include "redis_client.h"
#include "task.pb.h"
#include "zk_client.h"


using namespace taskscheduler;

enum class LoadBalanceType{
    ROUND_ROBIN, //轮询
    CPU_PRIORITY, //CPU 优先
    MEMORY_PRIORITY, //内存优先
    DISK_PRIORITY, //磁盘优先
    NETWORK_PRIORITY, //网速优先
    COMBINED_LOAD_BALANCE,//综合负载均衡
};

struct SchedulerNodeInfo{
    std::string ip;
    int port;
    bool undata_flag;  //只用于定时更新健康调度器表 删除标志位

    //CPU 使用率
    double cpu_usage;
    //内存使用率
    double memory_usage;
    //磁盘使用率
    double disk_usage;
    //网速使用率
    double network_usage;
};

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

    //从健康调度器表取出一个健康的调度器节点 -----后续负载均衡的引入位置
    std::pair<std::string,int> get_hearly_secheduler_node();

    //通过socket 提交任务到调度器
    void socket_submit_task_to_scheduler(taskscheduler::Task& task,std::pair<std::string,int>& scheduer_host);

    //轮询负载均衡
    std::pair<std::string,int> round_robin_load_balance();

    //CPU 优先负载均衡
    std::pair<std::string,int> cpu_priority_load_balance();

    //内存优先负载均衡
    std::pair<std::string,int> memory_priority_load_balance();

    //磁盘优先负载均衡
    std::pair<std::string,int> disk_priority_load_balance();

    //网速优先负载均衡
    std::pair<std::string,int> network_priority_load_balance();

    //综合负载均衡
    std::pair<std::string,int> combined_load_balance();
    
    //根据权重获取综合得分
    double get_combined_score(const SchedulerNodeInfo& node_info);


    ///////外部接口/////////
    //提交一个任务
    void submit_one_task(taskscheduler::Task task);
    //提交一组任务
    void submit_more_task(std::vector<taskscheduler::Task> taskarray);
    //提交一个定时任务
    void sunmit_one_timeout(taskscheduler::Task task,int timeout);

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

    

    //负载均衡方式
    LoadBalanceType load_balance_type_;
    //负载均衡权重
    std::unordered_map<std::string,double> load_balance_weight_;
    //轮询下标
    int round_robin_index_;




    
};

