#include "worker.h"

#include <dlfcn.h>
#include <array>
#include <fstream>
#include <json/json.h>
#include <thread>
// 工作器配置
const std::string WORKER_HOST = "127.0.0.1";
const int WORKER_PORT = 12346;

// 函数库
const char *FUNCTION_LIB = "libmyfuncs.so";
// 能力描述
const std::map<std::string, std::string> DEC = {
    {"cpu", "8"},
    {"memory", "8G"},
    {"disk", "50G"},
    {"network", "100M"},
};

Worker::Worker() : running_(false)
{
    init();
}

Worker::~Worker()
{
    stop();
}

void Worker::init()
{
    // 初始化zk客户端
    zkcli_ = std::make_shared<ZkClient>();
    zkcli_->connect(WORKER_ZK_ADDR);
    // 初始化节点
    std::string heartbeat_data;
    taskscheduler::WorkerHeartbeat heartbeat;
    heartbeat.set_worker_id(WORKER_ID);
    heartbeat.set_worker_ip(WORKER_HOST);
    heartbeat.set_worker_port(WORKER_PORT);
    heartbeat.set_timestamp(time(nullptr));
    heartbeat.set_is_healthy(true);
    // 将能力描述添加到心跳数据
    for (auto &item : DEC)
    {
        heartbeat.mutable_dec()->insert({item.first, item.second});
    }
    // 将节点数据序列化
    heartbeat.SerializeToString(&heartbeat_data);
    zkcli_->createNode(WORKER_ZK_PATH + "/" + WORKER_ID, heartbeat_data, ZOO_PERSISTENT);

    // 初始化工作端任务队列
    worker_task_queue_ = std::make_shared<MyWorkerTaskQueue>();
    worker_task_queue_->connect(RABBITMQ_HOST, RABBITMQ_PORT, RABBITMQ_USER, RABBITMQ_PASSWORD);
    // 初始化工作端任务结果队列
    worker_result_queue_ = std::make_shared<MyWorkerResultQueue>();
    worker_result_queue_->connect(RABBITMQ_HOST, RABBITMQ_PORT, RABBITMQ_USER, RABBITMQ_PASSWORD);
}

void Worker::start()
{
    if (running_)
        return;
    running_ = true;

    // 启动上报心跳线程
    report_heartbeat_thread_ = std::thread([this]()
                                           { report_heartbeat(); });

    // 启动接收任务线程
    receive_task_thread_ = std::thread([this]()
                                       { receive_task(); });
    // 启动执行任务线程
    exec_task_thread_ = std::thread([this]()
                                    { exec_task(); });
    // 启动消费任务结果线程
    consume_task_result_thread_ = std::thread([this]()
                                              { report_task_result(); });
}

void Worker::stop()
{
    if (!running_)
    {
        return;
    }
    return;
    running_ = false;
    // 停止上报心跳线程
    report_heartbeat_thread_.join();
    // 删除zk节点数据
    zkcli_->deleteNode(WORKER_ZK_PATH + "/" + WORKER_ID);
    // 停止接收任务线程
    if (receive_task_thread_.joinable())
        receive_task_thread_.join();
    // 停止执行任务线程
    if (exec_task_thread_.joinable())
        exec_task_thread_.join();
    // 停止消费任务结果线程
    if (consume_task_result_thread_.joinable())
        consume_task_result_thread_.join();

    // 关闭工作端任务队列
    if (worker_task_queue_)
        worker_task_queue_->close();
    // 关闭工作端任务结果队列
    if (worker_result_queue_)
        worker_result_queue_->close();
    // 关闭zk客户端
    if (zkcli_)
        zkcli_->close();
}

void Worker::receive_task()
{
    // 接收任务实现
    worker_task_queue_->consumeTask(
        [this](const taskscheduler::Task &task)
        {
            // 将任务添加到待执行任务缓存队列
            std::lock_guard<std::mutex> lock(pending_tasks_mutex_);
            pending_tasks_.push(task);
            // 通知执行任务线程
            pending_tasks_queue_not_empty_.notify_one();
        });
}

// 监听任务结果缓存队列
void Worker::report_task_result()
{
    taskscheduler::TaskResult task_result;
    while (running_)
    {
        std::unique_lock<std::mutex> lock(task_result_queue_mutex_);
        if (task_result_queue_.empty() && running_)
        {
            task_result_queue_not_empty_.wait(lock, [this]()
                                              { return !running_ || !task_result_queue_.empty(); });
        }
        if (!task_result_queue_.empty())
        {
            task_result = task_result_queue_.front();
            task_result_queue_.pop();
            // 上报任务结果
            worker_result_queue_->publishResult(task_result);
        }
        if (!running_ && task_result_queue_.empty())
        {
            break;
        }
    }
}
// 上报心跳实现
void Worker::report_heartbeat()
{

    while (running_)
    {
        // 组装心跳数据
        std::string heartbeat_data;
        taskscheduler::WorkerHeartbeat heartbeat;
        heartbeat.set_worker_id(WORKER_ID);
        heartbeat.set_worker_ip(WORKER_HOST);
        heartbeat.set_worker_port(WORKER_PORT);
        heartbeat.set_timestamp(time(nullptr));
        heartbeat.set_is_healthy(true);
        // 将能力描述添加到心跳数据
        for (auto &item : DEC)
        {
            heartbeat.mutable_dec()->insert({item.first, item.second});
        }
        // 将节点数据序列化
        heartbeat.SerializeToString(&heartbeat_data);
        // 修改zk节点数据
        zkcli_->setNodeData(WORKER_ZK_PATH + "/" + WORKER_ID, heartbeat_data);
        // 睡眠10秒
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

// 执行命令
std::string exec_cmd(const std::string &cmd)
{
    std::array<char, 128> buffer;
    std::string result;
    // "r" 代表只读
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        return "popen failed!";
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    {
        result += buffer.data();
    }
    int ret = pclose(pipe);
    if (ret != 0)
        return "";
    return result;
}
// 执行函数
std::string exec_func(const std::string &func, const std::map<std::string, std::string> &params)
{
    // 1. 打开动态库
    void *handle = dlopen(FUNCTION_LIB, RTLD_LAZY);
    if (!handle)
    {
        return std::string("dlopen failed: ") + dlerror();
    }

    // 2. 查找函数
    typedef const char *(*func_t)(const char *);
    func_t f = (func_t)dlsym(handle, func.c_str());
    if (!f)
    {
        dlclose(handle);
        return std::string("dlsym failed: ") + dlerror();
    }

    // 3. 参数转json字符串
    Json::Value root;
    for (const auto &kv : params)
    {
        root[kv.first] = kv.second;
    }
    Json::StreamWriterBuilder writer;
    std::string param_json = Json::writeString(writer, root);

    // 4. 调用函数
    const char *result = f(param_json.c_str());
    std::string ret = result ? result : "";

    // 5. 关闭动态库
    dlclose(handle);

    return ret;
}
// 执行任务实现
void Worker::exec_task()
{
    taskscheduler::TaskResult task_result;
    while (running_)
    {
        std::unique_lock<std::mutex> lock(pending_tasks_mutex_);
        if (pending_tasks_.empty() && running_)
        {
            pending_tasks_queue_not_empty_.wait(lock, [this]()
                                                { return !running_ || !pending_tasks_.empty(); });
        }
        if (!pending_tasks_.empty())
        {
            taskscheduler::Task task = pending_tasks_.front();
            pending_tasks_.pop();
            // 解析任务
            // 任务类型
            auto task_type = task.type();
            // 任务上下文
            std::string context = task.content();
            // 任务ID
            std::string task_id = task.task_id();
            // 任务参数
            std::map<std::string, std::string> params;
            // 填写任务参数表
            for (auto &item : task.metadata())
            {
                params[item.first] = item.second;
            }
            // 执行任务
            if (task_type == taskscheduler::TaskType::COMMAND   )
            {
                // 组装参数
                std::string command = task.content();
                for (auto &item : params)
                {
                    command += " " + item.first + " " + item.second;
                }
                // 执行命令
                std::string result = exec_cmd(command);
                if (result != "")
                {
                    task_result.set_output(result);
                    task_result.set_status(taskscheduler::TaskStatus::SUCCESS);
                }
                else
                {
                    task_result.set_error_message("命令执行失败");
                    task_result.set_status(taskscheduler::TaskStatus::FAILED);
                }
            }
            else if (task_type == taskscheduler::TaskType::FUNCTION)
            {
                // 执行函数
                std::string result = exec_func(context, params);
                if (result != "")
                {
                    task_result.set_output(result);
                    task_result.set_status(taskscheduler::TaskStatus::SUCCESS);
                }
                else
                {
                    task_result.set_error_message("函数执行失败");
                    task_result.set_status(taskscheduler::TaskStatus::FAILED);
                }
            }
            else
            {
                // 不支持的任务类型
                task_result.set_error_message("不支持的任务类型");
                task_result.set_status(taskscheduler::TaskStatus::FAILED);
            }
            // 设置任务结果
            task_result.set_task_id(task_id);
            task_result.set_start_time(time(nullptr));
            task_result.set_end_time(time(nullptr));
            task_result.set_worker_id(WORKER_ID);

            // 缓存任务结果
            {
                std::lock_guard<std::mutex> lock(task_result_queue_mutex_);
                task_result_queue_.push(task_result);
            }

            // 通知上报任务结果线程
            task_result_queue_not_empty_.notify_one();
        }
        if (!running_ && pending_tasks_.empty())
        {
            break;
        }
    }
}
