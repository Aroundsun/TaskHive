#include "scheduler.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

SchedConfig* config = SchedConfig::GetInstance("../config/scheduler_config.json");

// zk配置
const std::string ZK_HOST = config->get_zk_host();
const std::string ZK_ROOT_PATH = config->get_zk_root_path();
const std::string ZK_PATH = config->get_zk_path();
const std::string ZK_NODE = config->get_zk_node();

// redis配置
const std::string REDIS_HOST = config->get_redis_ip();
const int REDIS_PORT = config->get_redis_port();
const std::string REDIS_PASSWORD = config->get_redis_passwd();

// rabbitmq配置
const std::string RABBITMQ_HOST = config->get_rabbitmq_ip();
const int RABBITMQ_PORT = config->get_rabbitmq_port();
const std::string RABBITMQ_USER = config->get_rabbitmq_user();
const std::string RABBITMQ_PASSWORD = config->get_rabbitmq_password();
const int SCHEDULER_TASK_CHANNEL_ID = config->get_scheduler_task_channel_id();
const int SCHEDULER_RESULT_CHANNEL_ID = config->get_scheduler_result_channel_id();

// 能力描述
const std::unordered_map<std::string, std::string> DEC = config->get_dec();

// 心跳时间间隔（毫秒）
const int HEARTBEAT_INTERVAL = config->get_heartbeat_interval();

// 接收任务配置
const std::string RECEIVE_TASK_HOST = config->get_receive_task_host();
const int RECEIVE_TASK_PORT = config->get_receive_task_port();

scheduler::scheduler() : running_(false)
{
    init();
}

scheduler::~scheduler()
{
    stop();
}

void scheduler::start()
{
    // 启动调度器实现
    if (running_)
    {
        return;
    }
    running_ = true;
    // 启动上报心跳线程 同时注册zk 临时健康节点
    taskscheduler::SchedulerHeartbeat heartbeat;
    heartbeat.set_scheduler_id(ZK_NODE);
    heartbeat.set_scheduler_ip(RECEIVE_TASK_HOST);
    heartbeat.set_scheduler_port(RECEIVE_TASK_PORT);
    heartbeat.set_timetamp(time(nullptr));
    heartbeat.set_is_healthy(true);
    // 将能力描述添加到心跳数据
    for (auto &item : DEC)
    {
        heartbeat.mutable_dec()->insert({item.first, item.second});
    }

    std::string heartbeat_data;
    // 将节点数据序列化
    heartbeat.SerializeToString(&heartbeat_data);
    if (!zkcli_->createNode(ZK_PATH + "/" + ZK_NODE, heartbeat_data, ZOO_EPHEMERAL))
    {
        throw std::runtime_error("创建Zookeeper节点失败");
    }
    report_heartbeat_thread_ = std::thread(&scheduler::report_heartbeat_thread_function, this);
    // 启动获取任务线程
    receive_task_thread_ = std::thread(&scheduler::receive_task_thread_function, this);
    // 启动提交任务线程
    submit_task_thread_ = std::thread(&scheduler::submit_task_thread_function, this);
    // 启动获取任务结果线程
    get_task_result_thread_ = std::thread(&scheduler::get_task_result_thread_function, this);
}

// 初始化实现
void scheduler::init()
{
    try
    {
        //初始化监听套接字
        // 通过监听一个端口，接收任务
        //  创建一个socket
        listen_socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_socket_fd_ == -1)
        {
            throw std::runtime_error("创建socket失败");
        }

        // 设置socket选项，允许地址重用
        int opt = 1;
        setsockopt(listen_socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // 绑定端口
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(RECEIVE_TASK_PORT);
        addr.sin_addr.s_addr = inet_addr(RECEIVE_TASK_HOST.c_str());
        if (bind(listen_socket_fd_, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        {
            close(listen_socket_fd_);
            throw std::runtime_error("绑定端口失败");
        }
        // 监听端口
        if (listen(listen_socket_fd_, 10) == -1)
        {
            close(listen_socket_fd_);
            throw std::runtime_error("监听端口失败");
        }

        // 初始化zk客户端
        zkcli_ = std::make_shared<ZkClient>();
        if (!zkcli_->connect(ZK_HOST))
        {
            throw std::runtime_error("连接Zookeeper失败");
        }
        // 检查项目根节点是否存在
        if (!zkcli_->exists(ZK_PATH))
        {
            // 创建根节点
            if (!zkcli_->createNode(ZK_ROOT_PATH, "", ZOO_PERSISTENT))
            {
                throw std::runtime_error("创建Zookeeper项目根节点失败");
            }
            //创建调度器根节点
            if (!zkcli_->createNode(ZK_PATH, "", ZOO_PERSISTENT))
            {
                throw std::runtime_error("创建Zookeeper调度器根节点失败");
            }
        }

        // 初始化redis客户端
        rediscli_ = std::make_shared<RedisClient>();
        if (!rediscli_->connect(REDIS_HOST, REDIS_PORT, REDIS_PASSWORD))
        {
            std::cerr << "连接Redis失败: " << REDIS_HOST << ":" << REDIS_PORT << std::endl;
        }

        // 初始化任务队列
        scheduler_task_queue_ = std::make_unique<MySchedulerTaskQueue>(SCHEDULER_TASK_CHANNEL_ID);
        if (!scheduler_task_queue_->connect(RABBITMQ_HOST, RABBITMQ_PORT, RABBITMQ_USER, RABBITMQ_PASSWORD))
        {
            
            std::cerr << "连接RabbitMQ任务队列失败" << std::endl;
        }

        scheduler_task_result_queue_ = std::make_unique<MySchedulerResultQueue>(SCHEDULER_RESULT_CHANNEL_ID);
        if (!scheduler_task_result_queue_->connect(RABBITMQ_HOST, RABBITMQ_PORT, RABBITMQ_USER, RABBITMQ_PASSWORD))
        {
            std::cerr << "连接RabbitMQ结果队列失败" << std::endl;
        }

    }
    catch (const std::exception &e)
    {
        std::cerr << "初始化调度器失败: " << e.what() << std::endl;
    }
}

void scheduler::stop() {
    if (!running_) return;
    running_ = false;
    // 先关闭RabbitMQ channel，唤醒amqp_consume_message等阻塞线程
    if (scheduler_task_queue_){
        scheduler_task_queue_->close();
        std::cout << "关闭任务队列" << std::endl;
    }
    if (scheduler_task_result_queue_){
        scheduler_task_result_queue_->close();
        std::cout << "关闭结果队列" << std::endl;
    }
    // 唤醒所有等待的线程
    pending_tasks_queue_not_empty_.notify_all();
    // 主动唤醒阻塞在accept的线程
    if (listen_socket_fd_ != -1) {
        int tmpfd = socket(AF_INET, SOCK_STREAM, 0);
        if (tmpfd != -1) {
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(RECEIVE_TASK_PORT);
            addr.sin_addr.s_addr = inet_addr(RECEIVE_TASK_HOST.c_str());
            connect(tmpfd, (struct sockaddr*)&addr, sizeof(addr));
            close(tmpfd);
        }
        close(listen_socket_fd_);
        listen_socket_fd_ = -1;
    }
    // join所有线程，确保先关闭channel再join线程
    if (report_heartbeat_thread_.joinable()){
        report_heartbeat_thread_.join();
        std::cout << "上报心跳线程已停止" << std::endl;
    }
    if (receive_task_thread_.joinable()){
        receive_task_thread_.join();
        std::cout << "接收任务线程已停止" << std::endl;
    }
    if (submit_task_thread_.joinable()){
        submit_task_thread_.join();
        std::cout << "提交任务线程已停止" << std::endl;
    }
    if (get_task_result_thread_.joinable()){
        get_task_result_thread_.join();
        std::cout << "获取任务结果线程已停止" << std::endl;
    }

    // 关闭zk、redis等资源
    if (zkcli_){
        zkcli_->close();
        std::cout << "关闭zk客户端" << std::endl;
    }
    if (rediscli_){
        rediscli_->close();
        std::cout << "关闭redis客户端" << std::endl;
    }
    if (!pending_tasks_.empty())
    {
        std::cout << "待处理任务数量: " << pending_tasks_.size() << std::endl;
        // TODO: 实现分发: 将待执行的任务分发给一个健康的调度器节点
    }
    std::cout << "调度器已停止" << std::endl;
}

// 接收任务实现
void scheduler::receive_task_thread_function()
{
    try {
        while (running_){
            //debug
            std::cout<< "receive_task_thread_function 阻塞在accept" << running_ << std::endl;
            int connfd = accept(listen_socket_fd_, NULL, NULL);
            //debug
            std::cout<< "receive_task_thread_function 被唤醒在accept" << running_ << std::endl;
            if (connfd == -1){
                if (running_){
                    std::cerr << "接受连接失败: " << strerror(errno) << std::endl;
                    continue;
                }
                break;//退出循环
            }
            char buffer[4096];
            ssize_t len = read(connfd, buffer, sizeof(buffer));
            if (len == -1){
                std::cerr << "读取数据失败: " << strerror(errno) << std::endl;
                close(connfd);
                continue;
            }
            if (len == 0){
                close(connfd);
                continue;
            }
            taskscheduler::Task task;
            if (!task.ParseFromArray(buffer, len))
            {
                std::cerr << "解析任务失败" << std::endl;
                close(connfd);
                continue;
            }
            {
                //接收到一个任务，存入待执行队列
                std::lock_guard<std::mutex> lock(pending_tasks_mutex_);
                pending_tasks_.push(task);
            }
            pending_tasks_queue_not_empty_.notify_one();
        }
    } catch (const std::exception& e) {
        std::cerr << "接收任务线程异常: " << e.what() << std::endl;
    }
}
// 提交任务实现
void scheduler::submit_task_thread_function()
{
    try {
        taskscheduler::Task task;
        //debug
        std::cout << "submit_task_thread_function 开始提交任务" << running_ << std::endl;

        while (running_)
        {

            std::unique_lock<std::mutex> lock(pending_tasks_mutex_);
            if (pending_tasks_.empty() && running_)
            {
                //debug
                //这个线程被阻塞在这里
                std::cout << "submit_task_thread_function 接收任务线程被阻塞在这里" << std::endl;
                pending_tasks_queue_not_empty_.wait(lock, [this]() { return !running_ || !pending_tasks_.empty(); });
                //debug
                std::cout << "submit_task_thread_function 接收任务线程被唤醒" << std::endl;
            }
            if (!pending_tasks_.empty())
            {
                //debug
                std::cout << "submit_task_thread_function 提交任务" << std::endl;
                task = pending_tasks_.front();
                pending_tasks_.pop();
                scheduler_task_queue_->publishTask(task);
                //debug
                std::cout<<"提交的任务id 是 "<<task.task_id()<<std::endl;
            }
            if (!running_ && pending_tasks_.empty())
            {
                //debug
                std::cout << "submit_task_thread_function 退出循环" << std::endl;
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "提交任务线程异常: " << e.what() << std::endl;
    }

}
// 获取任务结果实现
void scheduler::get_task_result_thread_function()
{
    try
    {
        scheduler_task_result_queue_->consumeResult([this](taskscheduler::TaskResult &result){
            std::cout << "[Scheduler] 收到结果: " << result.task_id() << ", 状态: " << result.status() << std::endl;
            if(rediscli_->setTaskResult(result.task_id(), result, 3600)) {
                std::cout << "[Scheduler] 任务结果存储到Redis成功: " << result.task_id() << std::endl;
            } else {
                std::cerr << "[Scheduler] 任务结果存储到Redis失败: " << result.task_id() << std::endl;
            }
        });
    }
    catch (const std::exception &e)
    {
        std::cerr << "获取任务结果线程异常: " << e.what() << std::endl;
    }
}
// 上报心跳实现
void scheduler::report_heartbeat_thread_function()
{
    while (running_)
    {
        try
        {
            //上报健康心跳
            report_healthy_heartbeat_to_zk();
            // 心跳时间间隔 -可配置
            std::this_thread::sleep_for(std::chrono::milliseconds(HEARTBEAT_INTERVAL));
        }
        catch (const std::exception &e)
        {
            report_unhealthy_heartbeat_to_zk();
            throw std::runtime_error(std::string("上报心跳失败:")+e.what());
        }
    }
    //退出时上报不健康心跳
    report_unhealthy_heartbeat_to_zk();
}


//上报健康心跳实现
void scheduler::report_healthy_heartbeat_to_zk()
{
    //组装心跳数据
    taskscheduler::SchedulerHeartbeat heartbeat;
    heartbeat.set_scheduler_id(ZK_NODE);
    heartbeat.set_scheduler_ip(RECEIVE_TASK_HOST);
    heartbeat.set_scheduler_port(RECEIVE_TASK_PORT);
    heartbeat.set_timetamp(time(nullptr));
    heartbeat.set_is_healthy(true);
    // 将能力描述添加到心跳数据
    for (auto &item : DEC)
    {
        heartbeat.mutable_dec()->insert({item.first, item.second});
    }

    std::string heartbeat_data;
    // 将节点数据序列化
    heartbeat.SerializeToString(&heartbeat_data);
    if(zkcli_){
        // 修改zk节点数据
        zkcli_->setNodeData(ZK_PATH + "/" + ZK_NODE, heartbeat_data);
    }
    else{
        throw std::runtime_error("zkcli_ is null");
    }
}
//上报不健康心跳实现
void scheduler::report_unhealthy_heartbeat_to_zk()
{
    //组装心跳数据
    taskscheduler::SchedulerHeartbeat heartbeat;
    heartbeat.set_scheduler_id(ZK_NODE);
    heartbeat.set_scheduler_ip(RECEIVE_TASK_HOST);
    heartbeat.set_scheduler_port(RECEIVE_TASK_PORT);
    heartbeat.set_timetamp(time(nullptr));
    heartbeat.set_is_healthy(false);
    std::string heartbeat_data;
    // 将节点数据序列化
    heartbeat.SerializeToString(&heartbeat_data);
    if(zkcli_){
        // 修改zk节点数据
        zkcli_->setNodeData(ZK_PATH + "/" + ZK_NODE, heartbeat_data);
    }
    else{
        throw std::runtime_error("zkcli_ is null");
    }
}

