# TaskHive

## 项目简介

TaskHive 是一个基于 C++/RabbitMQ/Protobuf/Zookeeper/Redis 的分布式任务调度系统，支持任务、结果、心跳等多种高可靠消息队列，具备类型安全、异常处理、可扩展等特性。适用于大规模分布式任务调度、执行与监控场景。

### 消息队列设计

#### 1. **CRTP 模板模式 + 角色分离**
- **创新点**：采用 CRTP（Curiously Recurring Template Pattern）实现编译时多态，确保类型安全
- **设计亮点**：
  - `TaskQueue<Derived>`、`ResultQueue<Derived>`、`HeartbeatQueue<Derived>` 模板基类
  - 调度器侧：`MySchedulerTaskQueue`（发布任务）、`MySchedulerResultQueue`（消费结果）、`MySchedulerHeartbeatQueue`（消费心跳）
  - Worker侧：`MyWorkerTaskQueue`（消费任务）、`MyWorkerResultQueue`（发布结果）、`MyWorkerHeartbeatQueue`（发布心跳）
  - 角色职责清晰分离，避免误用

#### 2. **智能资源管理与内存优化**
- **创新点**：自动内存管理和缓冲区优化
- **设计亮点**：
  - 使用 `amqp_maybe_release_buffers(conn_)` 及时释放网络缓冲区，避免内存泄漏
  - 智能 envelope 生命周期管理：`amqp_destroy_envelope(&envelope)` 确保资源释放
  - 连接状态检查：每次操作前验证 `is_connected_` 和 `conn_` 状态

#### 3. **类型安全的消息属性系统**
- **创新点**：基于消息类型的差异化属性配置
- **设计亮点**：
  - **任务/结果队列**：`delivery_mode = 2`（持久化）+ 过期时间（2小时），确保重要数据不丢失
  - **心跳队列**：`delivery_mode = 1`（非持久化），轻量级实时通信
  - 消息类型标记：`props.type = "task"/"result"/"heartbeat"`，便于监控和路由
  - 时间戳属性：`props.timestamp = time(nullptr)`，支持消息追踪

#### 4. **异常安全与错误处理**
- **创新点**：分层异常处理机制
- **设计亮点**：
  - 连接层异常：连接失败、认证失败、通道打开失败
  - 序列化层异常：Protobuf 序列化/反序列化失败
  - 消息层布失败、消费失败、解析失败
  - 回调层异常：用户回调异常不影响异常：发消息确认

#### 5. **高性能消费模式**
- **创新点**：阻塞式消费 + 异步回调处理
- **设计亮点**：
  - 使用 `amqp_consume_message` 实现高效阻塞消费
  - 回调函数支持：`TaskCallback`、`ResultCallback`、`HeartbeatCallback`
  - 消息确认机制：`amqp_basic_ack` 确保消息处理完成
  - 优雅退出：检测连接状态，支持优雅关闭

#### 6. **Channel 隔离与并发安全**
- **创新点**：多 Channel 隔离设计
- **设计亮点**：
  - 任务队列：`TASK_CHANNEL_ID = 2`
  - 结果队列：`RESULT_CHANNEL_ID = 3`  
  - 心跳队列：`HEARTBEAT_CHANNEL_ID = 4`
  - 创建时进行检查，避免 Channel 冲突，支持高并发场景

#### 7. **可扩展的队列架构**


---

## 目录结构

```
TaskHive/
├── include/           # 头文件（调度器、Worker、消息队列等核心逻辑）
├── src/               # 源码实现
├── proto/             # Protobuf 消息定义及生成文件
├── test/              # 测试用例
├── CMakeLists.txt     # CMake 构建脚本
└── README.md
```

---

## 依赖环境

- C++14 或以上
- [protobuf](https://github.com/protocolbuffers/protobuf)（libprotobuf，protoc >= 3.0）
- [rabbitmq-c](https://github.com/alanxz/rabbitmq-c)（librabbitmq）
- [Zookeeper C client](https://zookeeper.apache.org/，libzookeeper_mt）
- [hiredis](https://github.com/redis/hiredis)（libhiredis）
- RabbitMQ 服务端（建议 3.x 及以上，需本地或远程可用）

安装示例（Ubuntu）：
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake protobuf-compiler libprotobuf-dev librabbitmq-dev libzookeeper-mt-dev libhiredis-dev rabbitmq-server
```

---

## Protobuf 消息结构

见 `proto/task.proto`，主要包括：

- `Task`：任务结构
- `TaskResult`：任务执行结果
- `Heartbeat`：Worker 心跳
- `WorkerRegistration`：Worker 注册信息

如需修改消息结构，编辑 `proto/task.proto` 后，执行：
```bash
protoc --cpp_out=./proto proto/task.proto
```

---

## 编译方法

推荐使用 g++ 或 CMake：

**手动编译测试用例：**
```bash
# 消息队列测试
g++ -std=c++17 -Iinclude -Iproto -o test/test_message_queue test/test_message_queue.cpp proto/task.pb.cc -lpthread -lrabbitmq -lprotobuf

# Redis 客户端测试
g++ -std=c++17 -Iinclude -o test/test_redis test/test_redis.cpp src/redis_client.cpp -lhiredis

# Zookeeper 客户端测试（需要 Zookeeper 服务）
g++ -std=c++17 -Iinclude -o test/test_zkclient test/test_zkclient.cpp src/zk_client.cpp -lzookeeper_mt -lpthread
```

**CMake（可自行扩展 CMakeLists.txt）：**
```bash
mkdir -p build && cd build
cmake ..
make
```

---

## 服务管理

### RabbitMQ 服务

启动 RabbitMQ 服务（如未启动）：
```bash
sudo systemctl start rabbitmq-server
```
如需查看状态：
```bash
sudo systemctl status rabbitmq-server
```

### Redis 服务

启动 Redis 服务：
```bash
sudo systemctl start redis-server
```

### Zookeeper 服务

启动 Zookeeper 服务：
```bash
# 需要先安装 Zookeeper
sudo apt-get install zookeeperd
sudo systemctl start zookeeper
```

---

## 测试用例说明

### 1. 消息队列全链路测试

`test/test_message_queue.cpp` 自动演示了如下完整流程：

- 任务链路：任务发布 → 任务消费 → 结果发布 → 结果消费
- 心跳链路：心跳发布 → 心跳消费

所有环节均有详细输出，线程安全，资源自动释放。运行示例：

```bash
./test/test_message_queue
```

输出示例：
```
===== 任务链路测试 =====
[调度器] 已发布任务: sched_t1
[Worker] 已消费任务: sched_t1
[Worker] 已发布结果: sched_t1
[调度器] 已消费结果: sched_t1 状态: 2
[Worker] 任务消费线程退出: WorkerTaskQueue: 消费消息失败
[调度器] 结果消费线程退出: SchedulerResultQueue: 消费消息失败
===== 心跳链路测试 =====
[Worker] 已发布心跳: worker-1
[调度器] 已消费心跳: worker-1
[调度器] 心跳消费线程退出: SchedulerHeartbeatQueue: 消费消息失败
```

### 2. Redis 客户端功能测试

`test/test_redis.cpp` 测试 Redis 缓存功能，包括：

- 任务结果缓存（setTaskResult/getTaskResult）
- Worker 心跳信息缓存（setWorkerHeartbeat/getWorkerHeartbeat）

运行示例：
```bash
./test/test_redis
```

输出示例：
```
setTaskResult OK
getTaskResult: success
setWorkerHeartbeat OK
getWorkerHeartbeat:
  ip: 192.168.1.100
  status: online
  timestamp: 1721000000
```

### 3. Zookeeper 客户端功能测试

`test/test_zkclient.cpp` 测试 Zookeeper 协调功能，包括：

- 节点创建、读取、更新、删除
- 节点存在性检查
- 数据持久化

运行示例：
```bash
./test/test_zkclient
```

输出示例：
```
连接 Zookeeper 成功
节点创建成功: /test_node
节点存在测试通过
节点数据: hello_zk
节点数据更新成功
更新后节点数据: zk_update
节点删除成功
节点不存在测试通过
```

---

## 常见问题

- **RabbitMQ 连接失败**：请确保服务已启动，端口/账号密码正确，防火墙未阻断 5672 端口。
- **Protobuf 相关符号未定义**：请确认已用 protoc 生成 .pb.cc/.pb.h 并编译进目标文件。
- **多线程段错误**：已优化为主线程统一 close，若有自定义多线程用法请注意资源同步。
- **Zookeeper 编译错误**：不存在 zoo_acreate、zoo_aset 等接口，可以加上 -DTHREADED 就可以解决。

---

## 贡献与扩展

欢迎提交 issue 或 PR，支持更多消息类型、分布式特性、监控与可视化等扩展。

---