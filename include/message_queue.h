#pragma once
#include <string>
#include <stdexcept>
#include "task.pb.h"
extern "C"
{
#include <amqp.h>
#include <amqp_tcp_socket.h>
}

// channel_id 常量集中管理
#define TASK_CHANNEL_ID 2
#define RESULT_CHANNEL_ID 3


// 消息队列基类
class MessageQueue
{
public:

    explicit MessageQueue(int channel_id) : channel_id_(channel_id) {
        if (channel_id != TASK_CHANNEL_ID && channel_id != RESULT_CHANNEL_ID ) {
            throw std::invalid_argument("MessageQueue: 非法的 channel_id");
        }
        conn_ = nullptr;
        is_connected_ = false;
    }
    virtual ~MessageQueue() {}
    
    virtual bool connect(const std::string &mq_host, int mq_port,
                         const std::string &mq_user, const std::string &mq_pass)
    {
        //保存连接信息
        mq_host_ = mq_host;
        mq_port_ = mq_port;
        mq_user_ = mq_user;
        mq_pass_ = mq_pass;
        //创建连接
        conn_ = amqp_new_connection();
        if(!conn_){
            throw std::runtime_error("MessageQueue: 创建连接失败");
            return false;
        }
        //创建socket
        amqp_socket_t* socket = amqp_tcp_socket_new(conn_);
        if(!socket)
        {
            throw std::runtime_error("MessageQueue: 创建socket失败");
            return false;
        }
        //连接RabbitMQ 
        if(amqp_socket_open(socket,mq_host_.c_str(),mq_port) != AMQP_STATUS_OK)
        {
            throw std::runtime_error("MessageQueue: 连接RabbitMQ 失败");
            return false;
        }
        //登陆RabbitMQ
        amqp_rpc_reply_t login_reply = amqp_login(conn_, 
                                                "/", 
                                                0, 
                                                128*1024, 
                                                30,  
                                                AMQP_SASL_METHOD_PLAIN,
                                                mq_user_.c_str(),
                                                mq_pass_.c_str()
        );
        if(login_reply.reply_type != AMQP_RESPONSE_NORMAL)
        {
            throw std::runtime_error("MessageQueue: 登陆RabbitMQ 失败");
        }
        //打开channel
        amqp_channel_open(conn_,channel_id_);
        amqp_rpc_reply_t channel_reply = amqp_get_rpc_reply(conn_);
        if(channel_reply.reply_type != AMQP_RESPONSE_NORMAL)
        {
            throw std::runtime_error("MessageQueue: 打开channel失败");
        }
        //声明队列
        amqp_bytes_t queueid = amqp_cstring_bytes(queue_name_.c_str());
        amqp_queue_declare_ok_t* queue_declare_reply = amqp_queue_declare(
                                                                    conn_, 
                                                                    channel_id_, 
                                                                    queueid, 
                                                                    0, 
                                                                    1, 
                                                                    0,
                                                                    0, 
                                                                    amqp_empty_table);
        if(!queue_declare_reply)
        {
            //获取错误信息
            amqp_rpc_reply_t reply = amqp_get_rpc_reply(conn_);
            if(reply.reply_type != AMQP_RESPONSE_NORMAL)
            {
                throw std::runtime_error("MessageQueue: 声明队列失败");
            }
        }
        //标记连接成功
        is_connected_ = true;

        return true;
    }
    
    virtual void close() {
        if (is_connected_ && conn_) {
            // 关闭 channel
            amqp_channel_close(conn_, channel_id_, AMQP_REPLY_SUCCESS);
            
            // 关闭连接
            amqp_connection_close(conn_, AMQP_REPLY_SUCCESS);
            
            // 销毁连接
            amqp_destroy_connection(conn_);
            
            // 重置状态
            conn_ = nullptr;
            is_connected_ = false;
        }
    }
    int getChannelId() const { return channel_id_; }
    amqp_connection_state_t getConnection() const { return conn_; }
protected:
    std::string queue_name_{""};

    std::string mq_host_{""};
    int mq_port_{0};
    std::string mq_user_{""};
    std::string mq_pass_{""};
    amqp_connection_state_t conn_;
    bool is_connected_;
    int channel_id_;
};

// 任务队列基类
template <typename Derived>
class TaskQueue : public MessageQueue
{
public:
// 任务队列消费回调
using TaskCallback = std::function<void(taskscheduler::Task &task)>;

protected:
    TaskQueue(int channel_id = TASK_CHANNEL_ID) : MessageQueue(channel_id) {
        if (channel_id != TASK_CHANNEL_ID) {
            throw std::invalid_argument("TaskQueue: 非法的 channel_id");
        }
        queue_name_ = "task_queue";
    }
    // 防止直接实例化
    virtual void mustOverride() const = 0;
public:
    virtual bool publishTask(const taskscheduler::Task &task)
    {
        return static_cast<Derived *>(this)->publishTaskImpl(task);
    }
    virtual bool consumeTask(TaskCallback callback)
    {
        return static_cast<Derived *>(this)->consumeTaskImpl(callback);
    }
};

// 结果队列基类
template <typename Derived>
class ResultQueue : public MessageQueue
{
public:
// 结果队列消费回调
using ResultCallback = std::function<void(taskscheduler::TaskResult &result)>;

protected:
    ResultQueue(int channel_id = RESULT_CHANNEL_ID) : MessageQueue(channel_id) {
        if (channel_id != RESULT_CHANNEL_ID) {
            throw std::invalid_argument("ResultQueue: 非法的 channel_id");
        }
        queue_name_ = "result_queue";
    }
    // 防止直接实例化
    virtual void mustOverride() const = 0;
public:
    virtual bool publishResult(const taskscheduler::TaskResult &result)
    {
        return static_cast<Derived *>(this)->publishResultImpl(result);
    }
    virtual bool consumeResult(ResultCallback callback) 
    {
        return static_cast<Derived *>(this)->consumeResultImpl(callback);
    }
};

// 工作器专用队列
class MyWorkerTaskQueue : public TaskQueue<MyWorkerTaskQueue>
{
public:
    MyWorkerTaskQueue(int channel_id = TASK_CHANNEL_ID) : TaskQueue(channel_id) {}
    bool publishTaskImpl(const taskscheduler::Task&) { throw std::runtime_error("Not implemented"); }
    bool consumeTaskImpl(TaskCallback task_cb){

        //检查连接状态
        if(!is_connected_ || !conn_)
        {
            throw std::runtime_error("consumeTaskImpl 未连接到RabbitMQ");
        }
        //声明队列
        //略 消息队列基类connect 函数中已经声明过   
        //重新声明队列确保存在
        // amqp_bytes_t queueid = amqp_cstring_bytes(queue_name_.c_str());
        // amqp_queue_declare_ok_t* queue_declare_reply = amqp_queue_declare(conn_, channel_id_, queueid, 0, 1, 0, 0, amqp_empty_table);
        // if(!queue_declare_reply)
        // {
        //     amqp_rpc_reply_t reply = amqp_get_rpc_reply(conn_);
        //     if(reply.reply_type != AMQP_RESPONSE_NORMAL)
        //     {
        //         throw std::runtime_error("WorkerTaskQueue: 声明队列失败");
        //         return false;
        //     }
        // }
        // std::cout << "WorkerTaskQueue: 队列声明成功: " << queue_name_ << std::endl;
        
        //注册消费者
        amqp_basic_consume_ok_t* consume_reply = amqp_basic_consume(conn_,
                                                                    channel_id_,
                                                                    amqp_cstring_bytes(queue_name_.c_str()),
                                                                    amqp_empty_bytes,
                                                                    0,
                                                                    1,
                                                                    0,
                                                                    amqp_empty_table
                                                                    );
        if(!consume_reply)
        {
            //获取错误信息
            amqp_rpc_reply_t reply = amqp_get_rpc_reply(conn_);
            if(reply.reply_type != AMQP_RESPONSE_NORMAL)
            {
                throw std::runtime_error("WorkerTaskQueue: 注册任务消费失败");
            }
        }
        //监听消费任务
        while(is_connected_){
            /*
            当成功消费完一条消息后，这些数据其实还在 buffer 里，没有立即释放；
            所以你需要调用 amqp_maybe_release_buffers(conn)，来尝试释放这些已经处理完的网络数据，节省内存。
            */
            amqp_maybe_release_buffers(conn_);
            //消费消息 -阻塞等待消息
            //debug
            std::cout<<"============等待任务消息。。。。。。。。"<<std::endl;
            //定义信封
            amqp_envelope_t envelope;
            memset(&envelope, 0, sizeof(envelope));
            //消费消息
            amqp_rpc_reply_t ret = amqp_consume_message(conn_, &envelope, nullptr, 0);

            std::cout<<"============收到任务消息。。。。。。。。"<<std::endl;
            std::cout <<__FILE__<<":"<<__LINE__<< " amqp_consume_message返回类型: " << ret.reply_type << std::endl;
            if(ret.reply_type == AMQP_RESPONSE_NORMAL){
                std::cout << "WorkerTaskQueue: 消息接收成功，开始解析" << std::endl;
                //解析消息
                std::string msg_body(static_cast<char*>(envelope.message.body.bytes), envelope.message.body.len);
                std::cout << "WorkerTaskQueue: 收到消息，大小: " << msg_body.size() << std::endl;
                std::cout << "WorkerTaskQueue: 消息内容(hex): ";
                for(size_t i = 0; i < std::min(msg_body.size(), size_t(50)); ++i) {
                    printf("%02x ", (unsigned char)msg_body[i]);
                }
                std::cout << std::endl;
                
                taskscheduler::Task task;
                if(!task.ParseFromString(msg_body)){
                    std::cerr << "WorkerTaskQueue: 解析消息失败，消息大小: " << msg_body.size() << std::endl;
                    throw std::runtime_error("WorkerTaskQueue: 解析消息失败");
                }
                std::cout << "WorkerTaskQueue: 消息解析成功，任务ID: " << task.task_id() << std::endl;
                // 处理消息
                try{
                    task_cb(task);//对消息的处理由使用者实现
                }
                catch(const std::exception& e){
                    std::cerr << "WorkerTaskQueue: 处理消息失败" << e.what() << std::endl;
                }
                // 确认消息
                if (amqp_basic_ack(conn_, channel_id_, envelope.delivery_tag, 0) != AMQP_STATUS_OK) {
                    std::cerr << "WorkerTaskQueue: Ack 消息失败" << std::endl;
                }
                //销毁envelope
                amqp_destroy_envelope(&envelope);
            }
            else {
                throw std::runtime_error("WorkerTaskQueue: 消费任务失败");
            }
        }
        //debug
        std::cout<<"============消费任务结束"<<std::endl;
        return true;
    }

    //空实现，防止基类被实例化
    void mustOverride() const override {}
};
class MyWorkerResultQueue : public ResultQueue<MyWorkerResultQueue>
{
public:
    MyWorkerResultQueue(int channel_id = RESULT_CHANNEL_ID) : ResultQueue(channel_id) {}
    bool publishResultImpl(const taskscheduler::TaskResult &result){
            //检查连接状态
            if(!is_connected_ ||!conn_){
                throw std::runtime_error("WorkerResultQueue: 未连接到RabbitMQ");
            }
            //序列化结果
            std::string serialized_result;
            if(!result.SerializeToString(&serialized_result)){
                throw std::runtime_error("WorkerResultQueue: 序列化结果失败");
            }
            //构造RabbitMQ需要的参数
            //队列名称
            amqp_bytes_t queueid = amqp_cstring_bytes(queue_name_.c_str());
            //消息内容
            amqp_bytes_t msg_bytes = amqp_cstring_bytes(serialized_result.c_str());
            // 消息属性
            amqp_basic_properties_t props;
            memset(&props, 0, sizeof(props));
            props._flags = AMQP_BASIC_DELIVERY_MODE_FLAG | 
                            AMQP_BASIC_EXPIRATION_FLAG   | 
                            AMQP_BASIC_TYPE_FLAG         | 
                            AMQP_BASIC_TIMESTAMP_FLAG;
            props.delivery_mode = 2; // 持久化消息 2:持久化消息 1:非持久化消息
            //为持久消息设置过期时间
            props.expiration = amqp_cstring_bytes("7200000"); // 2小时
            //设置消息类型
            props.type = amqp_cstring_bytes("result");
            //设置消息创建时间
            props.timestamp = time(nullptr);

            //发布消息
            int ret = amqp_basic_publish(
                                conn_,           
                                channel_id_,
                                amqp_empty_bytes, 
                                queueid, 
                                0, 
                                0, 
                                &props, 
                                msg_bytes);
    
            //检查发布结果
            if(ret < 0){
                throw std::runtime_error("WorkerResultQueue: 发布消息失败");
            }
    
            return true;
    }
    bool consumeResultImpl(ResultCallback) { throw std::runtime_error("Not implemented"); }
    void mustOverride() const override {}
};


#define TASK_EXPIRATION_TIME 7200000 // 2小时

// 调度器专用队列
class MySchedulerTaskQueue : public TaskQueue<MySchedulerTaskQueue>
{
public:
    MySchedulerTaskQueue(int channel_id = TASK_CHANNEL_ID) : TaskQueue(channel_id) {}
    bool publishTaskImpl(const taskscheduler::Task &task){
        //检查连接状态
        if(!is_connected_ ||!conn_){
            throw std::runtime_error("SchedulerTaskQueue: 未连接到RabbitMQ");
        }
        std::cout << "SchedulerTaskQueue: 开始发布任务" << std::endl;
        
        //序列化任务
        std::string serialized_task;
        if(!task.SerializeToString(&serialized_task)){
            throw std::runtime_error("SchedulerTaskQueue: 序列化任务失败");
        }
        std::cout << "SchedulerTaskQueue: 任务序列化成功，大小: " << serialized_task.size() << std::endl;
        
        //构造RabbitMQ需要的参数
        //队列名称
        amqp_bytes_t queueid = amqp_cstring_bytes(queue_name_.c_str());
        std::cout << "SchedulerTaskQueue: 队列名称: " << queue_name_ << std::endl;
        
        //消息内容
        amqp_bytes_t msg_bytes = amqp_cstring_bytes(serialized_task.c_str());
        std::cout << "SchedulerTaskQueue: 消息内容准备完成" << std::endl;
        
        // 消息属性
        amqp_basic_properties_t props;
        memset(&props, 0, sizeof(props));
        props._flags = AMQP_BASIC_DELIVERY_MODE_FLAG | 
                        AMQP_BASIC_EXPIRATION_FLAG   | 
                        AMQP_BASIC_TYPE_FLAG         | 
                        AMQP_BASIC_TIMESTAMP_FLAG;
        //设置消息持久化
        props.delivery_mode = 2; // 持久化消息 2:持久化消息 1:非持久化消息
        //为持久消息设置过期时间
        props.expiration = amqp_cstring_bytes(std::to_string(TASK_EXPIRATION_TIME).c_str()); // 1小时
        //设置消息类型
        props.type = amqp_cstring_bytes("task");    
        //设置消息创建时间
        props.timestamp = time(nullptr);
        std::cout << "SchedulerTaskQueue: 消息属性设置完成" << std::endl;

        //发布消息
        std::cout << "SchedulerTaskQueue: 开始调用amqp_basic_publish" << std::endl;
        
        // 先声明队列确保存在
        amqp_queue_declare_ok_t* queue_declare_reply = amqp_queue_declare(
            conn_, channel_id_, queueid, 0, 1, 0, 0, amqp_empty_table);
        if(!queue_declare_reply) {
            amqp_rpc_reply_t reply = amqp_get_rpc_reply(conn_);
            if(reply.reply_type != AMQP_RESPONSE_NORMAL) {
                throw std::runtime_error("SchedulerTaskQueue: 声明队列失败");
            }
        }
        std::cout << "SchedulerTaskQueue: 队列声明成功，消息数: " << queue_declare_reply->message_count << std::endl;
        
        // 使用amqp_bytes_malloc_dup来正确分配消息内存
        amqp_bytes_t message = amqp_bytes_malloc_dup(msg_bytes);
        
        // 使用简化的消息属性
        int ret = amqp_basic_publish(
                            conn_,           
                            channel_id_,
                            amqp_empty_bytes, 
                            queueid, 
                            0, 
                            0, 
                            nullptr, 
                            message);
        
        amqp_bytes_free(message);

        //检查发布结果
        if(ret < 0){
            throw std::runtime_error("SchedulerTaskQueue: 发布消息失败");
        }

        std::cout << "SchedulerTaskQueue: 任务发布成功" << std::endl;
        return true;
    }
    bool consumeTaskImpl(TaskCallback cb) { throw std::runtime_error("Not implemented"); }
    void mustOverride() const override {}
};
class MySchedulerResultQueue : public ResultQueue<MySchedulerResultQueue>
{
public:
    MySchedulerResultQueue(int channel_id = RESULT_CHANNEL_ID) : ResultQueue(channel_id) {}
    bool publishResultImpl(const taskscheduler::TaskResult&) { throw std::runtime_error("Not implemented"); }
    bool consumeResultImpl(ResultCallback result_cb)
    {
        //检查连接状态
        if(!is_connected_ || !conn_)
        {
            throw std::runtime_error("consumeResultImpl 未连接到RabbitMQ");
        }
        //声明队列
        //略 消息队列基类connect 函数中已经声明过
        //注册消费者

        amqp_basic_consume_ok_t* consume_reply = amqp_basic_consume(
                        conn_,
                        channel_id_,
                        amqp_cstring_bytes(queue_name_.c_str()),
                        amqp_empty_bytes,
                        0,
                        1,
                        0,
                        amqp_empty_table
                        );
        if(!consume_reply)
        {
            amqp_rpc_reply_t reply = amqp_get_rpc_reply(conn_);
            if(reply.reply_type != AMQP_RESPONSE_NORMAL)
            {
                throw std::runtime_error("SchedulerResultQueue: 注册任务结果消费者失败");
                return false;
            }
        }
        //监听消费结果
        while(is_connected_){
            /*
            当成功消费完一条消息后，这些数据其实还在 buffer 里，没有立即释放；
            所以你需要调用 amqp_maybe_release_buffers(conn)，来尝试释放这些已经处理完的网络数据，节省内存。
            */
            amqp_maybe_release_buffers(conn_);
            //定义信封
            amqp_envelope_t envelope;
            //消费消息 -阻塞等待消息
            amqp_rpc_reply_t ret = amqp_consume_message(conn_, &envelope, nullptr, 0);
            std::cout<<"============收到结果消息。。。。。。。。"<<std::endl;
            if(ret.reply_type == AMQP_RESPONSE_NORMAL){
                //解析消息
                taskscheduler::TaskResult result;
                std::string msg_body(static_cast<char*>(envelope.message.body.bytes), envelope.message.body.len);
                if(!result.ParseFromString(msg_body)){
                    throw std::runtime_error("SchedulerResultQueue: 解析消息失败");
                }
                // 处理消息
                try{
                    result_cb(result);//对消息的处理由使用者实现
                }
                catch(const std::exception& e){
                    std::cerr << "SchedulerResultQueue: 处理消息失败" << e.what() << std::endl;
                }
                // 确认消息
                if (amqp_basic_ack(conn_, channel_id_, envelope.delivery_tag, 0) != AMQP_STATUS_OK) {
                    std::cerr << "SchedulerResultQueue: Ack 消息失败" << std::endl;
                }
                //销毁envelope
                amqp_destroy_envelope(&envelope);
            }
            else {
                throw std::runtime_error("SchedulerResultQueue: 消费消息失败");
            }
        }

        return true;
    }

    void mustOverride() const override {}
};