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
解决办法是将工作器内部的通信使用json 外边一律使用 protobuf

## 问题六
一个任务下没有问题，但当第二个任务提交的时候，消费的接口出现消费失败，重新启动有可以消费，也就是说只能消费一次，应该是rabbitMQ 消费啥的
逻辑问题，比如conn_复用之类的，
解决办法：
查看rabbitMQ日志
2025-07-21 04:06:27.338305+00:00 [error] <0.8511.0> Channel error on connection <0.8502.0> (127.0.0.1:46916 -> 127.0.0.1:5672, vhost: '/', user: 'guest'), channel 3:
2025-07-21 04:06:27.338305+00:00 [error] <0.8511.0> operation basic.ack caused a channel exception precondition_failed: unknown delivery tag 1
2025-07-21 04:06:27.338594+00:00 [error] <0.8498.0> Channel error on connection <0.8489.0> (127.0.0.1:46904 -> 127.0.0.1:5672, vhost: '/', user: 'guest'), channel 2:
2025-07-21 04:06:27.338594+00:00 [error] <0.8498.0> operation basic.ack caused a channel exception precondition_failed: unknown delivery tag 1
问题原因应该是connect 的实现在基类中message_queue中，导致了在第二次消费过程中进行了 连接资源的复用，导致了 Channel error on connection、在调用amqp_basic_consume 注册消费者的时候将 ack 设置为1 消费者收到消息后，显式调用 amqp_basic_ack()两次，重复 ack 会导致 RabbitMQ 直接关闭该 channel，所以就有个library_error: -16，错误信息为 unexpected protocol state。 这个错误信息，
解决方法，
1、将基类中的connect 方法定义成纯虚函数，他的必须实现在其子类中，这样直接消除这个隐患，使得完全隔离connect 函数，这个虽然没有解决这个问题但是这也是一种优化。
1、去掉一个amqp_basic_ack()
## 问题七
在关闭调度器工作器、客户端的时候，开辟的线程不能正确的回收
解决方法：
1、先将socketfd 定义为数据成员，方便退出接收任务线程
2、资源释放顺序问题，导致有的线程中访问里已经访问的数据，导致段错误，调整资源释放顺序
3、将消息队列中的stop只负责修改表示，不释放资源，给消费阻塞哪里设置超时时间

## 问题八
在完善客户端负载均衡调度器心跳信息过程中，客户端无法读取到调度器心跳信息
terminate called after throwing an instance of 'std::runtime_error'
  what():  no healthy scheduler node!!!!
Aborted (core dumped)
解决方法
1、调度器心跳上传太慢，导致客户端第一时间读取不到心跳信息 -在调取器启动的时候先上传一个心跳
2、修复优化调度器关于心跳部分的逻辑
问题原因：客户端配置zk 路径问题

## 问题九
客户端在查询调度表的时候可以在还没有获取到调度器表 就开始提交任务，导致提交的第一个任务
解决办法：1、在客户端启动后等待5秒左右。----当前做法，后续在优化 
        2、优化客户端提交任务线程逻辑：在没有查到调度器host 就先阻塞
        
## 问题十
客户端向调度器多次提交任务相同id 的任务，调度器没有提交第二次上传的任务
解决办法：客户端主题交完任务轮询redes 
## 问题十一
当有多个任务调度器的时候，他获取了不是他提交的任务，导致这个任务结果被丢弃
问题原因：多个调度器使用相同的Channel ID，导致消息混乱
解决办法，修改配置文件，让channel 不冲突


