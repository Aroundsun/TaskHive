#pragma once
#include "message_queue.h"
#include "task.pb.h"
#include "redis_client.h"
#include "zk_client.h"

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
        //注册消费者
        amqp_basic_consume(conn_,
                        channel_id_,
                        amqp_cstring_bytes(queue_name_.c_str()),
                        amqp_empty_bytes,
                        0,
                        1,
                        0,
                        amqp_empty_table
                        );  
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
            if(ret.reply_type == AMQP_RESPONSE_NORMAL){
                //解析消息
                std::string msg_body(static_cast<char*>(envelope.message.body.bytes), envelope.message.body.len);
                taskscheduler::Task task;
                if(!task.ParseFromString(msg_body)){
                    throw std::runtime_error("WorkerTaskQueue: 解析消息失败");
                }
                // 处理消息
                try{
                    task_cb(task);//对消息的处理由使用者实现
                }
                catch(const std::exception& e){
                    std::cerr << "WorkerTaskQueue: 处理消息失败" << e.what() << std::endl;
                }
                // 确认消息
                amqp_basic_ack(conn_, channel_id_, envelope.delivery_tag, 0);
                //销毁envelope
                amqp_destroy_envelope(&envelope);
            }
            else {
                throw std::runtime_error("WorkerTaskQueue: 消费消息失败");
            }
        }
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
class MyWorkerHeartbeatQueue : public HeartbeatQueue<MyWorkerHeartbeatQueue>
{
public:
    MyWorkerHeartbeatQueue(int channel_id = HEARTBEAT_CHANNEL_ID) : HeartbeatQueue(channel_id) {}

        bool publishHeartbeatImpl(const taskscheduler::Heartbeat &hb){
            //检查连接状态
            if(!is_connected_ ||!conn_){
                throw std::runtime_error("WorkerHeartbeatQueue: 未连接到RabbitMQ");
            }
            //序列化心跳
            std::string serialized_heartbeat;
            if(!hb.SerializeToString(&serialized_heartbeat)){
                throw std::runtime_error("WorkerHeartbeatQueue: 序列化心跳失败");
            }
            //构造RabbitMQ需要的参数
            //队列名称
            amqp_bytes_t queueid = amqp_cstring_bytes(queue_name_.c_str());
            //消息内容
            amqp_bytes_t msg_bytes = amqp_cstring_bytes(serialized_heartbeat.c_str());
            // 消息属性
            amqp_basic_properties_t props;
            memset(&props, 0, sizeof(props));
            props._flags = AMQP_BASIC_DELIVERY_MODE_FLAG | 
                            AMQP_BASIC_TYPE_FLAG | 
                            AMQP_BASIC_TIMESTAMP_FLAG;
            props.delivery_mode = 1; // 非持久化消息 2:持久化消息 1:非持久化消息
            //设置消息类型
            props.type = amqp_cstring_bytes("heartbeat");
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
                throw std::runtime_error("WorkerHeartbeatQueue: 发布消息失败");
            }
    
            return true;
    }
    bool consumeHeartbeatImpl(HeartbeatCallback) { throw std::runtime_error("Not implemented"); }
    void mustOverride() const override {}
};


//执行期工作器
class Worker
{
public:
    Worker();
    ~Worker();

    bool init(const std::string &zk_addr, const std::string &mq_host, int mq_port,
              const std::string &mq_user, const std::string &mq_pass);

    //
    std::shared_ptr<MyWorkerTaskQueue> taskQueue_;
    std::shared_ptr<MyWorkerResultQueue> resultQueue_;
    std::shared_ptr<MyWorkerHeartbeatQueue> heartbeatQueue_;

    std::string workerId_;
    bool running_;  
};