#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <json/json.h>

// 负载均衡策略枚举
enum class LoadBalanceStrategy {
    ROUND_ROBIN = 0,           // 轮询
    LEAST_CONNECTIONS = 1,      // 最少连接数
    WEIGHTED_ROUND_ROBIN = 2,   // 加权轮询
    LEAST_RESPONSE_TIME = 3,    // 最快响应时间
    CONSISTENT_HASH = 4,        // 一致性哈希
    RANDOM = 5                  // 随机
};

// 工作器节点信息
struct WorkerNode {
    std::string worker_id;
    std::string worker_ip;
    int worker_port;
    std::map<std::string, std::string> capabilities;
    
    // 负载信息
    int current_connections;
    double cpu_usage;
    double memory_usage;
    double disk_usage;
    double network_speed;
    int response_time_ms;
    int weight;
    bool is_healthy;
    time_t last_heartbeat;
    
    // 统计信息
    int total_tasks_processed;
    int failed_tasks;
    double avg_response_time;
    
    WorkerNode();
};

// 负载均衡器接口
class ILoadBalancer {
public:
    virtual ~ILoadBalancer() = default;
    
    // 基本操作
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool is_running() const = 0;
    
    // 负载均衡策略
    virtual void set_strategy(LoadBalanceStrategy strategy) = 0;
    virtual LoadBalanceStrategy get_strategy() const = 0;
    virtual std::string get_strategy_name() const = 0;
    
    // 工作器管理
    virtual void add_worker(const std::string& worker_id, const std::string& worker_ip, int worker_port) = 0;
    virtual void remove_worker(const std::string& worker_id) = 0;
    virtual void update_worker_status(const std::string& worker_id, const std::map<std::string, std::string>& status) = 0;
    
    // 统计信息
    virtual Json::Value get_statistics() const = 0;
    virtual std::vector<std::shared_ptr<WorkerNode>> get_healthy_workers() const = 0;
    virtual int get_total_workers() const = 0;
    virtual int get_healthy_workers_count() const = 0;
    
    // 任务分发
    virtual std::string select_worker_for_task(const std::string& task_id, const std::map<std::string, std::string>& task_metadata) = 0;
    virtual void distribute_task(const std::string& task_id, const std::map<std::string, std::string>& task_metadata) = 0;
};

// 负载均衡器工厂
class LoadBalancerFactory {
public:
    static std::unique_ptr<ILoadBalancer> create_load_balancer(
        const std::string& zk_host,
        const std::string& zk_worker_path,
        const std::string& rabbitmq_host,
        int rabbitmq_port,
        const std::string& rabbitmq_user,
        const std::string& rabbitmq_password,
        int task_channel_id,
        int result_channel_id,
        LoadBalanceStrategy strategy = LoadBalanceStrategy::LEAST_CONNECTIONS
    );
};

// 负载均衡器配置
struct LoadBalancerConfig {
    std::string zk_host;
    std::string zk_worker_path;
    std::string rabbitmq_host;
    int rabbitmq_port;
    std::string rabbitmq_user;
    std::string rabbitmq_password;
    int task_channel_id;
    int result_channel_id;
    LoadBalanceStrategy strategy;
    
    // 健康检查配置
    int health_check_interval_ms;
    int heartbeat_timeout_seconds;
    double min_health_score;
    
    // 负载均衡配置
    int max_connections_per_worker;
    int task_queue_size;
    int worker_discovery_interval_ms;
    
    LoadBalancerConfig();
};

// 负载均衡器监控接口
class ILoadBalancerMonitor {
public:
    virtual ~ILoadBalancerMonitor() = default;
    
    virtual void on_worker_added(const std::string& worker_id) = 0;
    virtual void on_worker_removed(const std::string& worker_id) = 0;
    virtual void on_worker_health_changed(const std::string& worker_id, bool is_healthy) = 0;
    virtual void on_task_distributed(const std::string& task_id, const std::string& worker_id) = 0;
    virtual void on_task_failed(const std::string& task_id, const std::string& error_message) = 0;
};

// 负载均衡器统计信息
struct LoadBalancerStats {
    int total_workers;
    int healthy_workers;
    int total_connections;
    double avg_cpu_usage;
    double avg_memory_usage;
    double avg_response_time;
    int tasks_distributed;
    int tasks_failed;
    std::string current_strategy;
    
    LoadBalancerStats();
    Json::Value to_json() const;
};

// 工作器健康检查结果
struct WorkerHealthCheck {
    std::string worker_id;
    bool is_healthy;
    double health_score;
    std::map<std::string, double> metrics;
    time_t last_check_time;
    
    WorkerHealthCheck();
    Json::Value to_json() const;
};

// 负载均衡器事件
enum class LoadBalancerEvent {
    WORKER_ADDED,
    WORKER_REMOVED,
    WORKER_HEALTH_CHANGED,
    TASK_DISTRIBUTED,
    TASK_FAILED,
    STRATEGY_CHANGED,
    HEALTH_CHECK_FAILED
};

// 负载均衡器事件监听器
class ILoadBalancerEventListener {
public:
    virtual ~ILoadBalancerEventListener() = default;
    
    virtual void on_event(LoadBalancerEvent event, const std::map<std::string, std::string>& data) = 0;
};

// 负载均衡器管理器
class LoadBalancerManager {
private:
    std::unique_ptr<ILoadBalancer> load_balancer_;
    std::vector<std::shared_ptr<ILoadBalancerEventListener>> event_listeners_;
    std::shared_ptr<ILoadBalancerMonitor> monitor_;
    LoadBalancerConfig config_;
    std::atomic<bool> running_;
    
public:
    LoadBalancerManager(const LoadBalancerConfig& config);
    ~LoadBalancerManager();
    
    // 基本操作
    void start();
    void stop();
    bool is_running() const;
    
    // 配置管理
    void update_config(const LoadBalancerConfig& config);
    LoadBalancerConfig get_config() const;
    
    // 事件监听
    void add_event_listener(std::shared_ptr<ILoadBalancerEventListener> listener);
    void remove_event_listener(std::shared_ptr<ILoadBalancerEventListener> listener);
    
    // 监控
    void set_monitor(std::shared_ptr<ILoadBalancerMonitor> monitor);
    
    // 统计信息
    LoadBalancerStats get_stats() const;
    Json::Value get_detailed_stats() const;
    
    // 任务分发
    std::string distribute_task(const std::string& task_id, const std::map<std::string, std::string>& metadata);
    
private:
    void notify_event_listeners(LoadBalancerEvent event, const std::map<std::string, std::string>& data);
}; 