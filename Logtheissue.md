# 问题记录与解决

## 问题一
调度器主线程没有阻塞，很快就结束了，而在停止函数中先将running_ 设置为false 导致部分的线程不能正常的工作，比如
接收客户端的任务器请求的线程还没有开始接收客户端的tcp 连接就结束了
### 解决办法
让主线程阻塞，先别着急修改running_状态- 缓兵之计 后边可以检查检查调度器析构函数逻辑

## 问题二
注册zk 节点老是失败
### 解决办法
查看多错误码 -101 表示节点目录不存在
错误原因:父目录不存在，zk 不能多级创建，
解决方法：创建节点前检查根节点，不存在则创建

## 问题三
调度器将任务给rabbitMQ 工作器没有获取
问题原因：服务器配置有问题，主机名不一致
解决方法：先使用 hostname -s 查看当前主机名，如何看etc/hosts 是否有，没有则加上
## 问题四
WorkerTaskQueue: amqp_consume_message返回类型: 2
WorkerTaskQueue: amqp_consume_message失败，返回类型: 2
WorkerTaskQueue: library_error: -16
WorkerTaskQueue: 库异常
============调用函数
接收任务失败: WorkerTaskQueue: 消费消息失败============函数执行结果: {
        "data" : "3.000000",
        "message" : "\u6570\u5b57\u76f8\u52a0\u6210\u529f",
        "success" : true
}
问题原因
                //打印任务结果
                std::cout<<"taskid: "<<taskid<<" result: "<<result<<std::endl;

                taskscheduler::TaskResult taskresult;
                //反序列化任务结果
                taskresult.ParseFromString(result);
                std::cout<<"taskid: "<<taskid<<" result: "<<taskresult.output()<<std::endl;
通过调试，发现任务的分发、接收、执行，结果的缓存都没有问题，最后定位到 问题出自 客户端监听redis 结果的线程函数凡序列化失败
taskid: task-1 result: {
        "data" : "3.000000",
        "message" : "\u6570\u5b57\u76f8\u52a0\u6210\u529f",
        "success" : true
}
ParseFromString failed! 可能 result 不是合法 protobuf 数据。   result 是json 把它等成二进制串肯定失败序列话失败
解决办法 ,先解析json 在构造任务结果message 

## 问题五
问题四出现的问提虽然 已经解决，但是同时使用protobuf 和json 感觉有点混乱，所以需要调整，工作器执行结果的逻辑
