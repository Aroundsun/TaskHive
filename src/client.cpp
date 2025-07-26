#include"client.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#include <thread>
#include<json/json.h>
#include"client_config.h"
ClientConfig* config = ClientConfig::GetInstance("../config/client_config.json");
//zk配置
const std::string ZK_HOST = config->get_zk_host();
const std::string ZK_PATH = config->get_zk_path();
//redis配置
const std::string REDIS_HOST = config->get_redis_host();
const int REDIS_PORT = config->get_redis_port();
const std::string REDIS_PASSWORD = config->get_redis_password();

//构造函数
Client::Client():running_(false){
    init();
}

//析构函数
Client::~Client(){
    stop();
    
}

//初始化函数
void Client::init(){
    //初始化 zk
    zk_client_ = std::make_unique<ZkClient>();
    if(!zk_client_->connect(ZK_HOST))
    {
        throw std::runtime_error("zk init errno!!!!");
    }
    //初始化redis
    redis_client_ = std::make_unique<RedisClient>();
    if(!redis_client_->connect(REDIS_HOST,REDIS_PORT,REDIS_PASSWORD))
    {
        throw std::runtime_error("redis client connect erron!!!");
    }
}
//启动函数
void Client::start()
{
    
    if(running_)
        return;
    running_ = true;
    //启动更新健康调度器表的线程
    updata_secheduler_node_table_thread_ = std::thread(&Client::updata_secheduler_node_table_threadfunction, this);
    //启动提交任务线程
    submit_task_thread_ = std::thread(&Client::submit_task_threadfunction, this);
    //启动拉取任务结果线程
    get_task_result_thread_ = std::thread(&Client::consume_task_resultfunction, this);

    std::cout<<"客户端启动中......"<<'\n';
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout<<"客户端启动完成......"<<'\n';
}

/*
    停止运行
*/
void Client::stop()
{       
    if(!running_)
        return;
    running_ = false;
    //唤醒所有等待的线程
    submit_task_queue_no_empty_.notify_all();


    //等待提交任务线程停止
    if(submit_task_thread_.joinable())
    {
        submit_task_thread_.join();
        //打印日志
        std::cout << "提交任务线程已停止" << std::endl;
    }
    //等待拉取任务结果线程停止
    if(get_task_result_thread_.joinable())
    {
        get_task_result_thread_.join();
        //打印日志
        std::cout << "拉取任务结果线程已停止" << std::endl;
    }
    if(updata_secheduler_node_table_thread_.joinable())
    {
        updata_secheduler_node_table_thread_.join();
        //打印日志
        std::cout << "更新健康调度器节点表线程已停止" << std::endl;
    }
    //关闭zk 客户端
    if(zk_client_)
    {
        zk_client_->close();
        //打印日志
        std::cout << "关闭zk 客户端" << std::endl;
    }
    //关闭redis 客户端
    if(redis_client_)
    {
        redis_client_->close();
        //打印日志
        std::cout << "关闭redis 客户端" << std::endl;
    }

}

//提交任务线程函数
void Client::submit_task_threadfunction()
{
    while(running_)
    {
        std::unique_lock<std::mutex> lock_submit_task(submit_task_mutex_);
        if(submit_undistribution_task_.empty() && running_)
        {
            //等待待提交任务队列有任务
            submit_task_queue_no_empty_.wait(lock_submit_task,[this](){
                return !running_ || !submit_undistribution_task_.empty();
            });
        }

        if(!submit_undistribution_task_.empty())
        {
            //获取任务
            auto task = submit_undistribution_task_.front();
            submit_undistribution_task_.pop();
            //获取一个调度节点ip-port
            try{
                std::pair<std::string,int> hearly_scheduer_host = get_hearly_secheduler_node();
                while(hearly_scheduer_host.first == "")
                {
                    //持续的获取
                    hearly_scheduer_host = get_hearly_secheduler_node();
                }
                
                socket_submit_task_to_scheduler(task,hearly_scheduer_host);
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';//打印异常信息
            }
            //添加到已经提交任务队列
            {
                std::lock_guard<std::mutex> lock_distribution_taskid(distribution_taskid_mutex_);
                distribution_taskid_.push_back(task.task_id());
            }
            
        }
    }
}
/*
    拉取任务结果线程函数
*/
void Client::consume_task_resultfunction()
{
    //遍历已经提交的任务队列，向redis 查询任务结果

    while(running_)
    {
        //遍历已提交任务id
        std::cout << "[Client] 当前待查询任务数量: " << distribution_taskid_.size() << std::endl;
        for(auto& taskid : distribution_taskid_)
        {
            std::cout << "[Client] 查询任务结果: " << taskid << std::endl;
            //从redis 查询任务结果
            std::string result = redis_client_->getTaskResult(taskid);
            if(result != "NO_RESULT")
            {
                std::cout << "[Client] 找到任务结果: " << taskid << std::endl;
                //查到结果删除
                redis_client_->deleteTaskResult(taskid);
                //反序列化任务结果
                taskscheduler::TaskResult taskresult;
                if(taskresult.ParseFromString(result)) {
                    std::cout << "[Client] 任务结果反序列化成功: " << taskid << ", 状态: " << taskresult.status() << std::endl;
                    {
                        //添加到任务结果队列
                        std::lock_guard<std::mutex> lock_taskresult(taskresult_mutex_);
                        taskresult_[taskid] = taskresult;
                    }

                    //从已提交任务id队列中移除
                    distribution_taskid_.erase(std::remove(distribution_taskid_.begin(),distribution_taskid_.end(),taskid),distribution_taskid_.end());
                    //通知获取结果线程有任务结果
                    submit_task_queue_no_empty_.notify_one();
                } else {
                    std::cerr << "[Client] 任务结果反序列化失败: " << taskid << std::endl;
                }
            }
            else
            {
                std::cout << "[Client] 未找到任务结果: " << taskid << std::endl;
            }
        }
        //等待1000ms 避免频繁查询redis
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}



 /*
    维护所有调度器节点，定时更新线程
*/
void Client::updata_secheduler_node_table_threadfunction()
{
    while(running_)
    {
        //遍历zk上的所有的调取器节点路径
        std::cout<<"准备获取zk 节点路径"<<std::endl;
        for(auto& scheduler_node_path : zk_client_->getAllNode(ZK_PATH))
        {
            //debug

            std::cout<<scheduler_node_path<<std::endl;
            //获取当前调度器节点的信息
            taskscheduler::SchedulerHeartbeat scheduler_heartbeat;

            std::string scheduler_info= zk_client_->getNodeData(ZK_PATH + '/' + scheduler_node_path);
            scheduler_heartbeat.ParseFromString(scheduler_info);

            std::string scheduler_id = scheduler_heartbeat.scheduler_id();

            //筛选出客户端想知道的调度器信息
            SchedulerNodeInfo scheduler_host;
            scheduler_host.ip = scheduler_heartbeat.scheduler_ip();
            scheduler_host.port = scheduler_heartbeat.scheduler_port();
            scheduler_host.undata_flag = true;

            //将能力描述添加到调度器节点信息
            for(auto& item : scheduler_heartbeat.dec())
            {       
                scheduler_host.descriptor[item.first] = item.second;
            }    
            //将当前的调度器节点信息缓存到本地
            {    
                std::lock_guard<std::mutex> lock_hearly_secheduler_node_table(hearly_secheduler_node_table_mutex_);
                hearly_secheduler_node_table_[scheduler_id] = scheduler_host;
            }
        }
        if(!running_)
        {
            break;
        }
        //等待1000ms 再进行下一次更新
        std::this_thread::sleep_for(std::chrono::seconds(10));
        { //每一次更新前调取表前，删除无效调取器及节点，重置所有有效调度器更新标识
            std::lock_guard<std::mutex> lock_hearly_secheduler_node_table(hearly_secheduler_node_table_mutex_);
            //遍历本地缓存的调度节点表 设置更新标识 为false
            //移除没有更新的调度器节点- 已经不存在
            for(auto it = hearly_secheduler_node_table_.begin();it != hearly_secheduler_node_table_.end();)
            {
                if(it->second.undata_flag == false){
                    it = hearly_secheduler_node_table_.erase(it); //返回下一个有效迭代器
                }
                else{  //当前的调取器信息更新过（有效的），重置更新标识
                    it->second.undata_flag = false;
                    ++it;
                }
            }
        }

    } 

}

//从健康调度器表取出一个健康的调度器节点 -----后续负载均衡的引入位置
std::pair<std::string,int> Client::get_hearly_secheduler_node()
{
    //获取健康调度器表
    std::lock_guard<std::mutex> lock_hearly_secheduler_node_table(hearly_secheduler_node_table_mutex_);
    if(hearly_secheduler_node_table_.empty())
    {
        throw std::runtime_error("no healthy scheduler node!!!!");
    }
    //返回一个健康调度器节点 -- 返回ip和port
    //取出这个节点
    auto it = hearly_secheduler_node_table_.begin()->second;
    return {it.ip,it.port};
}
///////////////外部接口//////////////////
//提交一个任务
void Client::submit_one_task(taskscheduler::Task task)
{
    //缓存任务到待提交队列
    std::unique_lock<std::mutex> lock_submit_task(submit_task_mutex_);
    submit_undistribution_task_.push(task);
    lock_submit_task.unlock();
    submit_task_queue_no_empty_.notify_one();
}

//提交多个任务
void Client::submit_more_task(std::vector<taskscheduler::Task> taskarray)
{
    std::unique_lock<std::mutex> lock_submit_task(submit_task_mutex_);
    for(auto& task : taskarray)
    {
        submit_undistribution_task_.push(task);
    }
    lock_submit_task.unlock();
    submit_task_queue_no_empty_.notify_all();
}

//通过socket 提交任务到调度器
void Client::socket_submit_task_to_scheduler(taskscheduler::Task& task,std::pair<std::string,int>& scheduer_host)
{
    //  开始提交
    std::cout<<"scheduer_host" << scheduer_host.first<<" "<<scheduer_host.second <<std::endl;
    
    //通过socket到调度器
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd == -1)
    {
        throw std::runtime_error("socket create error!!!");
    }
    //连接调度器
    // 创建一个sockaddr_in 结构体
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(scheduer_host.second);
    server_addr.sin_addr.s_addr = inet_addr(scheduer_host.first.c_str());
    // 连接调度器
    if(connect(sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr)) == -1)
    {
        throw std::runtime_error("connect to scheduler error!!!");
    }
    //任务序列化成string
    std::string task_str;
    task.SerializeToString(&task_str);
    std::cout<<"task_str: "<<task_str<<std::endl;
    //发送任务到调度器  
    if(send(sockfd,task_str.c_str(),task_str.size(),0) == -1)
    {
        throw std::runtime_error("send task to scheduler error!!!");
    }
    //关闭socket
    close(sockfd);
}

//根据一个taskid 来查询一个任务结果
taskscheduler::TaskResult Client::get_task_result(std::string taskid)const
{
    //查询结果
    std::cout<<"开始查询"<<std::endl;
    if(taskresult_.find(taskid) == taskresult_.end())
    {
        std::cout<<"查不到"<<std::endl;
        return taskscheduler::TaskResult();
    }
    return taskresult_.at(taskid);
}

//获取当前所有的任务执行结果
std::vector<taskscheduler::TaskResult> Client::get_all_task_result()
{
    std::lock_guard<std::mutex> lock_taskresult(taskresult_mutex_);
    std::vector<taskscheduler::TaskResult> taskresult_vector;
    for(auto& taskresult : taskresult_)
    {
        taskresult_vector.push_back(taskresult.second);
    }
    return taskresult_vector;
}

//查询一个任务结果并移除
taskscheduler::TaskResult Client::get_task_result_and_delete(std::string taskid) {
    std::lock_guard<std::mutex> lock_taskresult(taskresult_mutex_);
    if(taskresult_.find(taskid) == taskresult_.end())
    {
        return taskscheduler::TaskResult();
    }
    taskscheduler::TaskResult taskresult = taskresult_.at(taskid);
    taskresult_.erase(taskid);
    return taskresult;
}

//查询当前所有的任务结果并移除
std::vector<taskscheduler::TaskResult> Client::get_all_task_result_and_delete() {
    std::lock_guard<std::mutex> lock_taskresult(taskresult_mutex_);
    std::vector<taskscheduler::TaskResult> taskresult_vector;
    for(auto& taskresult : taskresult_)
    {
        taskresult_vector.push_back(taskresult.second);
    }
    taskresult_.clear();
    return taskresult_vector;   
}