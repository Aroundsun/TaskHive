#include "scheduler.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>


//待配置信息
//zk配置
const std::string ZK_HOST = "127.0.0.1:2181";
const std::string ZK_PATH = "TaskHive/schedulers";
const std::string ZK_NODE = "scheduler-1";
//redis配置
const std::string REDIS_HOST = "127.0.0.1";
const int REDIS_PORT = 6379;
const std::string REDIS_PASSWORD = "";

//rabbitmq配置
const std::string RABBITMQ_HOST = "127.0.0.1";
const int RABBITMQ_PORT = 5672;
const std::string RABBITMQ_USER = "guest";
const std::string RABBITMQ_PASSWORD = "guest";

//能力描述
const std::map<std::string, std::string> DEC = {
    {"cpu", "10"},
    {"memory", "10G"},
    {"disk", "100G"},
    {"network", "100M"},
};


//接收任务配置
const std::string RECEIVE_TASK_HOST = "127.0.0.1";
const int RECEIVE_TASK_PORT = 12345;

scheduler::scheduler() : running_(false) {
    init();
}

scheduler::~scheduler() {
    stop();
}

void scheduler::start() {
    // 启动调度器实现
    if (running_) {
        return;
    }
    running_ = true;
    // 启动上报心跳线程
    report_heartbeat_thread_ = std::thread(&scheduler::report_heartbeat, this);
    // 启动获取任务线程
    receive_task_thread_ = std::thread(&scheduler::receive_task, this);
    // 启动提交任务线程
    submit_task_thread_ = std::thread(&scheduler::submit_task, this);
    // 启动获取任务结果线程
    get_task_result_thread_ = std::thread(&scheduler::get_task_result, this);

}
// 初始化实现
void scheduler::init() {
    // 初始化zk客户端
    zkcli_ = std::make_shared<ZkClient>();
    zkcli_->connect(ZK_HOST);
    // 初始化redis客户端
    rediscli_ = std::make_shared<RedisClient>();
    rediscli_->connect(REDIS_HOST, REDIS_PORT, REDIS_PASSWORD);
    // 初始化任务队列
    scheduler_task_queue_ = std::make_shared<MySchedulerTaskQueue>();
    scheduler_task_queue_->connect(RABBITMQ_HOST, RABBITMQ_PORT, RABBITMQ_USER, RABBITMQ_PASSWORD);
    scheduler_task_result_queue_ = std::make_shared<MySchedulerTaskResultQueue>();
    scheduler_task_result_queue_->connect(RABBITMQ_HOST, RABBITMQ_PORT, RABBITMQ_USER, RABBITMQ_PASSWORD);

}

void scheduler::stop() {
    // 停止调度器实现
    running_ = false;
    if (report_heartbeat_thread_.joinable()) {
        report_heartbeat_thread_.join();
    }
    if (receive_task_thread_.joinable()) {
        receive_task_thread_.join();
    }
    if (submit_task_thread_.joinable()) {
        submit_task_thread_.join();
    }
    if (get_task_result_thread_.joinable()) {
        get_task_result_thread_.join();
    }
    zkcli_->close();
    rediscli_->close();
    scheduler_task_queue_->close();
    scheduler_task_result_queue_->close();
    if(!pending_tasks_.empty())
    {
        //可以将这些待执行的任务分发给一个健康的调度器节点
        //TODO: 实现分发
        //将这些待执行的任务分发给一个健康的调度器节点

        pending_tasks_.clear();
    }
}
// 接收任务实现
void scheduler::receive_task() {

    //通过监听一个端口，接收任务
    // 创建一个socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return;
    }
    // 绑定端口
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(RECEIVE_TASK_PORT);
    addr.sin_addr.s_addr = inet_addr(RECEIVE_TASK_HOST.c_str());
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(sockfd);
        return;
    }
    // 监听端口
    if (listen(sockfd, 10) == -1) { 
        perror("listen");
        close(sockfd);
        return;
    }
    // 接受任务
    while (running_) {
        int connfd = accept(sockfd, NULL, NULL);
        if (connfd == -1) {
            perror("accept");
            continue;
        }
        // 读取任务
        char buffer[1024];
        ssize_t len = read(connfd, buffer, sizeof(buffer));
        if (len == -1) {
            perror("read");     
            close(connfd);
            continue;
        }
        // 解析任务
        taskscheduler::Task task;
        if (!task.ParseFromArray(buffer, len)) {
            perror("parse");
            close(connfd);
            continue;
        }
        // 将任务添加到待分发的任务队列
        {
            std::lock_guard<std::mutex> lock(pending_tasks_mutex_);
            pending_tasks_.push(task);
            //通知待提交的任务队列有任务
            pending_tasks_queue_not_empty_.notify_one();
        }
    }
    close(sockfd);
}
// 提交任务实现
void scheduler::submit_task() {
    //从待提交的任务队列中获取任务
    taskscheduler::Task task;
    while(running_)
    {
        std::unique_lock<std::mutex> lock(pending_tasks_mutex_);
        if(pending_tasks_.empty() && running_)
        {
            //等待待提交的任务队列有任务
            pending_tasks_queue_not_empty_.wait(lock, [this](){
                return !running_ || !pending_tasks_.empty();
            });
        }
        if(!pending_tasks_.empty())
        {
            //获取任务
            task = pending_tasks_.front();
            pending_tasks_.pop();
            //提交任务到rabbitmq
            scheduler_task_queue_->publishTask(task);
        }
        if(!running_ && pending_tasks_.empty())
        {
            break;
        }
    }

}
// 获取任务结果实现
void scheduler::get_task_result() {
    //消费rabbitmq中的任务结果
    scheduler_task_result_queue_->consumeTaskResult([this](taskscheduler::TaskResult& result){
        //将任务结果保存到redis
        rediscli_->setTaskResult(result.task_id(), result.result(), 3600);
    });
    
}
// 上报心跳实现
void scheduler::report_heartbeat() {
    /*
        string scheduler_id = 1; 
        string scheduler_ip = 2;
        int32 scheduler_port = 3;
        int32 timetamp = 4;
        bool is_healthy = 5;
        map<string,string> dec = 6;   //能力描述 
    }
    */
    //组装心跳数据
    taskscheduler::SchedulerHeartbeat heartbeat;
    heartbeat.set_scheduler_id(ZK_NODE);
    heartbeat.set_scheduler_ip(RECEIVE_TASK_HOST);
    heartbeat.set_scheduler_port(RECEIVE_TASK_PORT);
    heartbeat.set_timetamp(time(nullptr));
    heartbeat.set_is_healthy(true);
    //将能力描述添加到心跳数据
    for(auto& item : DEC)
    {
        heartbeat.mutable_dec()->insert({item.first, item.second});
    }
    
    
    //将心跳数据序列化
    std::string heartbeat_data;
    heartbeat.SerializeToString(&heartbeat_data);
    while(running_)
    {
        //上报心跳到zk
        zkcli_->setNodeData(ZK_PATH + "/" + ZK_NODE, heartbeat_data);  
        //等待10秒
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

}


