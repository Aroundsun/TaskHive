#include "worker.h"

#include <dlfcn.h>
#include <array>
#include <fstream>
#include <json/json.h>
#include <thread>

#include"worker_config.h"
// 函数库
const char *FUNCTION_LIB = "../lib/libmyfuncs.so";
WorkerConfig* config = WorkerConfig::GetInstance("../config/worker_config.json");

const std::string WORKER_HOST = config->get_worker_ip();
const int WORKER_PORT = config->get_worker_port();
const std::string WORKER_ID = config->get_worker_id();

const std::string WORKER_ZK_ADDR = config->get_zk_host();
const std::string ZK_ROOT_PATH = config->get_zk_root_path();
const std::string ZK_PATH = config->get_zk_path();
const std::string ZK_NODE = config->get_zk_node();

const std::string RABBITMQ_HOST = config->get_rabbitmq_ip();
const int RABBITMQ_PORT = config->get_rabbitmq_port();
const std::string RABBITMQ_USER = config->get_rabbitmq_user();
const std::string RABBITMQ_PASSWORD = config->get_rabbitmq_password();
const int WORKER_TASK_CHANNEL_ID = config->get_task_channel_id();
const int WORKER_RESULT_CHANNEL_ID = config->get_result_channel_id();

auto DEC = config->get_capabilities();


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
    if (!zkcli_->connect(WORKER_ZK_ADDR))
    {
        std::cerr << "连接Zookeeper失败: " << WORKER_ZK_ADDR << std::endl;
        return;
    }
    // 检查根节点是否存在
    if (!zkcli_->exists(ZK_PATH))
    {
        // 创建根节点
        if (!zkcli_->createNode(ZK_ROOT_PATH, "", ZOO_PERSISTENT))
        {
            std::cerr << "创建项目根节点失败" << std::endl;
            return;
        }
        // 创建节点
        if (!zkcli_->createNode(ZK_PATH, "", ZOO_PERSISTENT))
        {
            std::cerr << "创建worker根节点失败" << std::endl;
            return;
        }
    }
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
    // 创建当前工作端节点 --临时节点
    if (!zkcli_->createNode(ZK_PATH + "/" + ZK_NODE, heartbeat_data, ZOO_EPHEMERAL))
    {
        std::cerr << "创建Zookeeper节点失败" << std::endl;
        return;
    }

    // 初始化工作端任务队列
    worker_task_queue_ = std::make_unique<MyWorkerTaskQueue>(WORKER_TASK_CHANNEL_ID);
    if (!worker_task_queue_->connect(RABBITMQ_HOST, RABBITMQ_PORT, RABBITMQ_USER, RABBITMQ_PASSWORD))
    {
        std::cerr << "连接RabbitMQ任务队列失败" << std::endl;
        return;
    }
    // 初始化工作端任务结果队列
    worker_result_queue_ = std::make_unique<MyWorkerResultQueue>(WORKER_RESULT_CHANNEL_ID);
    if (!worker_result_queue_->connect(RABBITMQ_HOST, RABBITMQ_PORT, RABBITMQ_USER, RABBITMQ_PASSWORD))
    {
        std::cerr << "连接RabbitMQ结果队列失败" << std::endl;
        return;
    }
}

void Worker::start()
{
    if (running_)
        return;
    running_ = true;

    // 启动上报心跳线程
    report_heartbeat_thread_ = std::thread(&Worker::report_heartbeat, this);

    // 启动接收任务线程
    receive_task_thread_ = std::thread(&Worker::receive_task, this);
    // 启动执行任务线程
    exec_task_thread_ = std::thread(&Worker::exec_task, this);
    // 启动消费任务结果线程
    consume_task_result_thread_ = std::thread(&Worker::report_task_result, this);
}

void Worker::stop()
{
    if (!running_)
    {
        return;
    }
    running_ = false;
    // 唤醒所有等待的线程
    pending_tasks_queue_not_empty_.notify_all();
    task_result_queue_not_empty_.notify_all();
    // 关闭工作端任务队列
    if (worker_task_queue_)
    {
        worker_task_queue_->close();
    }
    // 关闭工作端任务结果队列
    if (worker_result_queue_)
    {
        worker_result_queue_->close();
    }
    // 停止上报心跳线程
    if (report_heartbeat_thread_.joinable())
    {
        report_heartbeat_thread_.join();
        // 打印日志
        std::cout << "上报心跳线程已停止" << std::endl;
    }

    // 停止接收任务线程
    if (receive_task_thread_.joinable())
    {
        receive_task_thread_.join();
        // 打印日志
        std::cout << "接收任务线程已停止" << std::endl;
    }
    // 停止执行任务线程
    if (exec_task_thread_.joinable())
    {
        exec_task_thread_.join();
        // 打印日志
        std::cout << "执行任务线程已停止" << std::endl;
    }
    // 停止消费任务结果线程
    if (consume_task_result_thread_.joinable())
    {
        consume_task_result_thread_.join();
        // 打印日志
        std::cout << "消费任务结果线程已停止" << std::endl;
    }

    // 关闭zk客户端
    if (zkcli_)
    {
        zkcli_->close();
        // 打印日志
        std::cout << "关闭zk客户端" << std::endl;
    }
}

// 消费任务实现
void Worker::receive_task()
{

    // 接收任务实现
    try
    {
        worker_task_queue_->consumeTask(
            [this](const taskscheduler::Task &task)
            {
                // 缓存任务到本地任务队列
                {
                    std::lock_guard<std::mutex> lock(pending_tasks_mutex_);
                    pending_tasks_.push(task);
                    // debug
                    std::cout << " 接收的任务的id:" << task.task_id() << std::endl;
                }
                // 通知执行任务线程
                pending_tasks_queue_not_empty_.notify_one();
            });
    }
    catch (const std::exception &e)
    {
        // 打印日志
        std::cerr << "接收任务失败: " << e.what() << std::endl;
    }
}

// 生产任务结果实现
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
        // 任务结果队列不为空
        if (!task_result_queue_.empty())
        {
            task_result = task_result_queue_.front();
            task_result_queue_.pop();
            // 上报任务结果
            worker_result_queue_->publishResult(task_result);
            // debug
            std::cout << " 上传的任务结果的id:" << task_result.task_id() << std::endl;
        }
        // 任务结果队列为空 且 运行状态为false 退出循环
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
        zkcli_->setNodeData(ZK_PATH + "/" + ZK_NODE, heartbeat_data);
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
    // 打开动态库
    void *handle = dlopen(FUNCTION_LIB, RTLD_LAZY);
    if (!handle)
    {
        return std::string("dlopen failed: ") + dlerror();
    }

    // 查找函数
    typedef const char *(*func_t)(const char *);
    func_t f = (func_t)dlsym(handle, func.c_str());
    if (!f)
    {
        dlclose(handle);
        return std::string("dlsym failed: ") + dlerror();
    }

    // 参数转json字符串
    Json::Value root;
    for (const auto &kv : params)
    {
        root[kv.first] = kv.second;
    }
    Json::StreamWriterBuilder writer;
    std::string param_json = Json::writeString(writer, root);
    // 调用函数
    const char *result = f(param_json.c_str());
    std::string ret = result ? result : "";

    // 关闭动态库
    dlclose(handle);
    return ret;
}
// 执行任务实现
void Worker::exec_task()
{
    taskscheduler::TaskResult task_result;
    while (running_)
    {
        // debug
        // debug
        std::cout << "执行线程等待锁" << std::endl;
        std::unique_lock<std::mutex> lock(pending_tasks_mutex_);
        std::cout << "执行线程获取到锁" << std::endl;

        if (pending_tasks_.empty() && running_)
        {
            // debug
            // debug
            std::cout << "执行线程被阻塞" << std::endl;
            pending_tasks_queue_not_empty_.wait(lock, [this]()
                                                { return !running_ || !pending_tasks_.empty(); });
            ////debug
            std::cout << "执行任务线程被唤醒了" << std::endl;
        }
        // 如果运行状态为false，退出循环
        if (!running_)
        {
            break;
        }
        // 如果任务队列不为空且运行状态为true，执行任务
        if (!pending_tasks_.empty())
        {
            taskscheduler::Task task = pending_tasks_.front();
            // debug
            std::cout << " 一个待任务的id:" << task.task_id() << std::endl;
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
            if (task_type == taskscheduler::TaskType::COMMAND)
            {
                // debug
                std::cout << " 命令的任务的id:" << task.task_id() << std::endl;
                // 组装参数
                std::string command = task.content();
                for (auto &item : params)
                {
                    command += " " + item.first + item.second;
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
                std::cout << " 函数的任务的id:" << task.task_id() << std::endl;

                // 执行函数
                std::string result = exec_func(context, params);
                // 现在的result是json字符串，需要反序列化
                Json::Value root;
                Json::CharReaderBuilder reader;
                Json::CharReader *reader_ptr = reader.newCharReader();
                if (!reader_ptr->parse(result.c_str(), result.c_str() + result.size(), &root, nullptr))
                {
                    std::cerr << "JSON 解析失败: " << std::endl;
                }
                std::string output = root["data"].asString();
                std::string error_message = root["message"].asString();
                bool success = root["success"].asBool();
                delete reader_ptr; // 释放内存
                // 反序列化任务结果
                if (success)
                {
                    task_result.set_output(output);
                    task_result.set_status(taskscheduler::TaskStatus::SUCCESS);
                }
                else
                {
                    task_result.set_error_message(error_message);
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
    }
}
