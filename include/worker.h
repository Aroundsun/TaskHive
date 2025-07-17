#pragma once
#include "message_queue.h"
#include "task.pb.h"
#include "redis_client.h"
#include "zk_client.h"


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