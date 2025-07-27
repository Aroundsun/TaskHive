# TaskHive - 分布式任务调度系统

TaskHive 是一个基于 C++ 开发的分布式任务调度系统，支持任务分发、负载均衡、健康监控等功能。

## 📋 功能清单

### ✅ 已完成功能

#### 核心功能
- [x] 分布式任务调度系统架构
- [x] 多组件通信 (Client ↔ Scheduler ↔ Worker)
- [x] 基于Zookeeper的服务发现
- [x] 基于RabbitMQ的任务分发
- [x] 基于Redis的结果缓存
- [x] 健康心跳监控机制

#### 负载均衡
- [x] 轮询负载均衡 (Round Robin)
- [x] CPU优先负载均衡
- [x] 内存优先负载均衡
- [x] 磁盘优先负载均衡
- [x] 网络优先负载均衡
- [x] 综合负载均衡 (权重计算)

#### 任务管理
- [x] 任务提交和分发
- [x] 任务执行状态跟踪
- [x] 任务结果收集和存储
- [x] 任务ID唯一性保证

#### 系统监控
- [x] 调度器健康状态监控
- [x] 工作器能力描述上报
- [x] 实时负载信息收集
- [x] 服务可用性检测

#### 测试覆盖
- [x] Redis连接测试
- [x] Zookeeper服务发现测试
- [x] RabbitMQ消息队列测试
- [x] 负载均衡算法测试

### 🚧 开发中功能

#### 多调度器支持
- [ ] 动态端口分配机制
- [ ] 唯一节点名生成
- [ ] 独立Chel IannD管理
- [ ] 调度器间任务转移

#### 高可用性
- [ ] 调度器故障自动切换
- [ ] 工作器故障自动重试
- [ ] 任务执行失败重试机制
- [ ] 服务降级策略

### 📋 计划功能

#### 容错机制完善
- [ ] 任务执行失败重试机制
- [ ] 调度器故障自动切换
- [ ] 工作器故障自动重试
- [ ] 网络异常重连机制
- [ ] 数据一致性保证

#### 任务执行优化
- [ ] 任务执行时间监控
- [ ] 任务超时处理机制
- [ ] 任务执行状态实时更新
- [ ] 任务执行性能分析

#### 线程池集成
- [ ] 线程池组件集成
- [ ] 任务执行线程池管理
- [ ] 线程池性能优化
- [ ] 线程池监控和统计

#### 日志系统
- [ ] 结构化日志记录
- [ ] 日志级别管理
- [ ] 日志轮转机制
- [ ] 日志查询和分析

#### 网络通信优化
- [ ] 网络连接池优化
- [ ] 数据传输压缩
- [ ] 网络异常处理
- [ ] 网络性能监控

#### 任务管理增强
- [ ] 任务优先级机制
- [ ] 定时任务提交
- [ ] 任务依赖关系管理
- [ ] 批量任务处理

#### 负载均衡优化
- [ ] 工作器+消息队列双端接收任务
- [ ] 负载均衡算法优化
- [ ] 动态负载调整
- [ ] 负载预测机制

#### 拓扑类任务
- [ ] 拓扑任务定义
- [ ] 拓扑任务执行引擎
- [ ] 拓扑任务状态管理
- [ ] 拓扑任务可视化

#### 系统监控增强
- [ ] 实时性能监控
- [ ] 资源使用统计
- [ ] 告警机制
- [ ] 监控数据可视化

## 📋 项目概述

### 核心组件
- **调度器 (Scheduler)**: 负责任务接收、分发和结果收集
- **工作器 (Worker)**: 执行具体任务并返回结果
- **客户端 (Client)**: 提交任务和获取结果
- **协调服务**: 使用 Zookeeper 进行服务发现和健康监控
- **消息队列**: 使用 RabbitMQ 进行任务分发
- **缓存服务**: 使用 Redis 存储任务结果

### 技术栈
- **语言**: C++17
- **序列化**: Protocol Buffers + JSON
- **消息队列**: RabbitMQ
- **缓存**: Redis
- **服务发现**: Zookeeper
- **构建系统**: CMake

## 🏗️ 系统架构

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Client    │    │  Scheduler  │    │   Worker    │
│             │    │             │    │             │
│ • 任务提交   │───▶│ • 任务接收   │───▶│ • 任务执行   │
│ • 结果获取   │◀───│ • 负载均衡   │◀───│ • 结果返回   │
└─────────────┘    └─────────────┘    └─────────────┘
       │                   │                   │
       └───────────────────┼───────────────────┘
                           │
                    ┌─────────────┐
                    │ Zookeeper   │
                    │ • 服务发现   │
                    │ • 健康监控   │
                    └─────────────┘
```

## 🚀 快速开始

### 环境要求

- **操作系统**: Linux (Ubuntu 18.04+)
- **编译器**: GCC 7.0+ 或 Clang 6.0+
- **依赖服务**:
  - Zookeeper 3.6+
  - RabbitMQ 3.8+
  - Redis 5.0+
  - CMake 3.10+

### 安装依赖

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y build-essential cmake libprotobuf-dev protobuf-compiler
sudo apt install -y libhiredis-dev libzookeeper-mt-dev librabbitmq-dev
sudo apt install -y libjsoncpp-dev libssl-dev

# 启动依赖服务
sudo systemctl start zookeeper
sudo systemctl start rabbitmq-server
sudo systemctl start redis-server
```

### 编译项目

```bash
# 克隆项目
git clone <repository-url>
cd TaskHive

# 创建构建目录
mkdir build && cd build

# 配置和编译
cmake ..
make -j$(nproc)

# 编译测试程序 (可选)
cmake -DBUILD_TESTS=ON ..
make -j$(nproc)
```

### 配置服务

1. **修改配置文件**:
   ```bash
   # 调度器配置
   vim config/scheduler_config.json
   
   # 工作器配置  
   vim config/worker_config.json
   
   # 客户端配置
   vim config/client_config.json
   ```

2. **启动服务**:
   ```bash
   # 启动调度器
   ./bin/scheduler &
   
   # 启动工作器
   ./bin/worker &
   
   # 启动客户端
   ./bin/client &
   ```

3. **清理进程**:
   ```bash
   ./kill_all.sh
   ```

## 📖 使用指南

### 基本使用流程

1. **启动所有服务**
2. **提交任务**: 通过客户端API提交任务
3. **监控执行**: 查看任务执行状态
4. **获取结果**: 从Redis获取任务结果

### 负载均衡策略

客户端支持多种负载均衡策略：

- **轮询 (Round Robin)**: `load_balance_type: 0`
- **CPU优先**: `load_balance_type: 1`
- **内存优先**: `load_balance_type: 2`
- **磁盘优先**: `load_balance_type: 3`
- **网络优先**: `load_balance_type: 4`
- **综合负载**: `load_balance_type: 5`

### 配置说明

#### 调度器配置 (scheduler_config.json)
```json
{
    "zk": {
        "host": "127.0.0.1:2181",
        "root_path": "/TaskHive",
        "path": "/TaskHive/schedulers",
        "node": "scheduler-1"
    },
    "redis": {
        "host": "127.0.0.1",
        "port": 6379,
        "password": ""
    },
    "rabbitmq": {
        "host": "127.0.0.1",
        "port": 5672,
        "user": "guest",
        "password": "guest",
        "task_channel_id": 1,
        "result_channel_id": 2
    },
    "capabilities": {
        "cpu": "10",
        "memory": "10G",
        "disk": "100G",
        "network": "100M"
    },
    "heartbeat_interval": 10000,
    "receive_task": {
        "host": "127.0.0.1",
        "port": 12345
    }
}
```

## 🧪 测试

### 运行测试程序

```bash
# 编译测试
cmake -DBUILD_TESTS=ON ..
make

# 运行测试
./bin/test_redis
./bin/test_zkclient  
./bin/test_message_queue
```

### 测试覆盖

- **Redis连接测试**: 验证Redis客户端功能
- **Zookeeper测试**: 验证服务发现功能
- **消息队列测试**: 验证RabbitMQ通信功能

## 🔧 开发指南

### 项目结构

```
TaskHive/
├── src/                    # 源代码
│   ├── scheduler.cpp      # 调度器实现
│   ├── worker.cpp         # 工作器实现
│   ├── client.cpp         # 客户端实现
│   └── ...
├── include/               # 头文件
│   ├── scheduler.h
│   ├── worker.h
│   ├── client.h
│   └── ...
├── config/                # 配置文件
├── proto/                 # Protocol Buffers定义
├── test/                  # 测试代码
├── examples/              # 示例代码
└── bin/                   # 编译输出
```

### 添加新功能

1. **添加新的负载均衡策略**:
   - 在 `src/client.cpp` 中添加新的负载均衡函数
   - 在 `include/client.h` 中声明函数
   - 在 `get_hearly_secheduler_node()` 中添加新的case

2. **添加新的任务类型**:
   - 在 `proto/task.proto` 中定义新的消息类型
   - 在 `src/function_lib.cpp` 中实现任务处理逻辑

## 🐛 常见问题

### 1. 端口冲突
**问题**: 多个调度器启动时出现端口绑定失败
**解决**: 为每个调度器配置不同的端口号

### 2. Zookeeper节点冲突
**问题**: 多个调度器使用相同节点名导致覆盖
**解决**: 为每个调度器配置唯一的节点名

### 3. RabbitMQ Channel错误
**问题**: 多个调度器使用相同Channel ID导致消息混乱
**解决**: 为每个调度器配置不同的Channel ID

### 4. 负载均衡逻辑错误
**问题**: 选择负载最重的节点而不是最轻的
**解决**: 修改负载均衡算法，选择资源使用率最低的节点

### 5. 线程回收问题
**问题**: 关闭服务时线程无法正确回收
**解决**: 优化资源释放顺序，确保线程正确退出

## 📝 更新日志

### v1.0.0
- 实现基本的分布式任务调度功能
- 支持多种负载均衡策略
- 集成Zookeeper服务发现
- 集成RabbitMQ消息队列
- 集成Redis结果缓存

## 🤝 贡献指南

1. Fork 项目
2. 创建功能分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 打开 Pull Request




## 📞 联系方式

如有问题或建议，请通过以下方式联系：
- 提交 Issue
- 发送邮件至: [xhyefg@163.com]

---

**注意**: 这是一个开发中的项目，可能存在一些已知问题。详细的问题记录请查看 `Logtheissue.md` 文件。
