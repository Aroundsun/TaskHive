# TaskHive 测试说明

## 概述

本项目包含以下测试程序：
- Redis 客户端测试
- Zookeeper 客户端测试  
- 消息队列测试
- 函数库测试

## 前置要求

### 1. 安装依赖库

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libjsoncpp-dev \
    libhiredis-dev \
    libzookeeper-mt-dev \
    pkg-config
```

### 2. 启动服务

#### Redis 服务
```bash
# 启动 Redis 服务
redis-server

# 或者使用系统服务
sudo systemctl start redis-server
```

#### Zookeeper 服务
```bash
# 下载并配置 Zookeeper
wget https://downloads.apache.org/zookeeper/zookeeper-3.7.1/apache-zookeeper-3.7.1-bin.tar.gz
tar -xzf apache-zookeeper-3.7.1-bin.tar.gz
cd apache-zookeeper-3.7.1-bin

# 创建配置文件
cp conf/zoo_sample.cfg conf/zoo.cfg

# 启动 Zookeeper
bin/zkServer.sh start
```

## 运行测试

### 方法1：使用测试脚本（推荐）

```bash
# 运行所有测试
./test/run_tests.sh
```

### 方法2：手动构建和运行

```bash
# 创建构建目录
mkdir -p build
cd build

# 配置和编译
cmake ..
make -j$(nproc)

# 运行各个测试
./test_redis
./test_zkclient
./test_message_queue
```

## 测试内容

### Redis 客户端测试 (test_redis)

测试内容：
- ✅ 基本操作测试（setTaskResult/getTaskResult）
- ✅ 多个任务测试
- ✅ 过期时间测试
- ✅ 连接错误处理测试

预期输出：
```
=== Redis 客户端测试程序 ===
正在连接 Redis...
✓ 连接 Redis 成功
=== 测试基本操作 ===
✓ setTaskResult 成功
✓ getTaskResult 成功: {"status":"success","output":"Hello World","timestamp":1234567890}
...
```

### Zookeeper 客户端测试 (test_zkclient)

测试内容：
- ✅ 基本操作测试（创建、读取、更新、删除节点）
- ✅ 层级节点测试
- ✅ 错误处理测试
- ✅ 并发操作测试
- ✅ 连接错误处理测试

预期输出：
```
=== Zookeeper 客户端测试程序 ===
正在连接 Zookeeper...
✓ 连接 Zookeeper 成功
=== 测试基本操作 ===
✓ 节点创建成功: /test_basic_node
✓ 节点存在测试通过
✓ 节点数据获取成功: hello_zk_basic
...
```

### 消息队列测试 (test_message_queue)

测试内容：
- ✅ 消息发布/订阅测试
- ✅ 任务结果队列测试
- ✅ 并发操作测试

## 故障排除

### 常见问题

1. **Redis 连接失败**
   ```
   错误：连接 Redis 失败
   解决：确保 Redis 服务正在运行
   ```

2. **Zookeeper 连接失败**
   ```
   错误：连接 Zookeeper 失败
   解决：确保 Zookeeper 服务正在运行
   ```

3. **编译错误**
   ```
   错误：找不到 hiredis.h
   解决：安装 libhiredis-dev 包
   ```

4. **库文件找不到**
   ```
   错误：找不到 libzookeeper_mt.so
   解决：安装 libzookeeper-mt-dev 包
   ```

### 调试技巧

1. **检查服务状态**
   ```bash
   # 检查 Redis
   redis-cli ping
   
   # 检查 Zookeeper
   echo stat | nc localhost 2181
   ```

2. **查看日志**
   ```bash
   # Redis 日志
   tail -f /var/log/redis/redis-server.log
   
   # Zookeeper 日志
   tail -f apache-zookeeper-3.7.1-bin/logs/zookeeper.out
   ```

3. **网络连接测试**
   ```bash
   # 测试 Redis 端口
   telnet localhost 6379
   
   # 测试 Zookeeper 端口
   telnet localhost 2181
   ```

## 测试覆盖率

当前测试覆盖了以下功能：

- [x] Redis 客户端基本功能
- [x] Zookeeper 客户端基本功能
- [x] 消息队列基本功能
- [x] 错误处理和异常情况
- [x] 并发操作
- [x] 连接管理

## 扩展测试

如需添加新的测试用例，请：

1. 在 `test/` 目录下创建新的测试文件
2. 在 `CMakeLists.txt` 中添加测试程序配置
3. 更新 `test/run_tests.sh` 脚本
4. 更新此文档

## 性能测试

对于性能测试，可以：

1. 增加并发测试用例
2. 添加压力测试
3. 监控内存和CPU使用情况
4. 测试大规模数据处理能力 