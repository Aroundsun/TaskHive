#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <random>
#include <chrono>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <json/json.h>
#include "zk_client.h"
#include "task.pb.h"
#include "message_queue.h"

// 负载均衡策略枚举
enum class LoadBalanceStrategy {
    ROUND_ROBIN,      // 轮询
    LEAST_CONNECTIONS, // 最少连接数
    WEIGHTED_ROUND_ROBIN, // 加权轮询
    LEAST_RESPONSE_TIME,  // 最快响应时间
    CONSISTENT_HASH,   // 一致性哈希
    RANDOM            // 随机
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
    
    WorkerNode() : current_connections(0), cpu_usage(0.0), memory_usage(0.0), 
                   disk_usage(0.0), network_speed(0.0), response_time_ms(0), 
                   weight(1), is_healthy(true), last_heartbeat(0),
                   total_tasks_processed(0), failed_tasks(0), avg_response_time(0.0) {}
};

// 负载均衡器类
class LoadBalancer {
private:
    std::vector<std::shared_ptr<WorkerNode>> worker_nodes_;
    std::mutex worker_nodes_mutex_;
    std::atomic<int> current_round_robin_index_;
    
    std::shared_ptr<ZkClient> zk_client_;
    std::unique_ptr<MySchedulerTaskQueue> task_queue_;
    std::unique_ptr<MySchedulerResultQueue> result_queue_;
    
    LoadBalanceStrategy strategy_;
    std::atomic<bool> running_;
    
    // 配置参数
    std::string zk_host_;
    std::string zk_worker_path_;
    std::string rabbitmq_host_;
    int rabbitmq_port_;
    std::string rabbitmq_user_;
    std::string rabbitmq_password_;
    int task_channel_id_;
    int result_channel_id_;
    
    // 监控线程
    std::thread worker_discovery_thread_;
    std::thread health_check_thread_;
    std::thread load_balancing_thread_;
    
    // 一致性哈希相关
    std::map<uint32_t, std::shared_ptr<WorkerNode>> hash_ring_;
    std::mutex hash_ring_mutex_;
    
public:
    LoadBalancer(const std::string& zk_host, const std::string& zk_worker_path,
                 const std::string& rabbitmq_host, int rabbitmq_port,
                 const std::string& rabbitmq_user, const std::string& rabbitmq_password,
                 int task_channel_id, int result_channel_id,
                 LoadBalanceStrategy strategy = LoadBalanceStrategy::LEAST_CONNECTIONS)
        : current_round_robin_index_(0), strategy_(strategy), running_(false),
          zk_host_(zk_host), zk_worker_path_(zk_worker_path),
          rabbitmq_host_(rabbitmq_host), rabbitmq_port_(rabbitmq_port),
          rabbitmq_user_(rabbitmq_user), rabbitmq_password_(rabbitmq_password),
          task_channel_id_(task_channel_id), result_channel_id_(result_channel_id) {
        init();
    }
    
    ~LoadBalancer() {
        stop();
    }
    
    void init() {
        // 初始化Zookeeper客户端
        zk_client_ = std::make_shared<ZkClient>();
        if (!zk_client_->connect(zk_host_)) {
            throw std::runtime_error("连接Zookeeper失败: " + zk_host_);
        }
        
        // 初始化RabbitMQ队列
        task_queue_ = std::make_unique<MySchedulerTaskQueue>(task_channel_id_);
        if (!task_queue_->connect(rabbitmq_host_, rabbitmq_port_, rabbitmq_user_, rabbitmq_password_)) {
            throw std::runtime_error("连接RabbitMQ任务队列失败");
        }
        
        result_queue_ = std::make_unique<MySchedulerResultQueue>(result_channel_id_);
        if (!result_queue_->connect(rabbitmq_host_, rabbitmq_port_, rabbitmq_user_, rabbitmq_password_)) {
            throw std::runtime_error("连接RabbitMQ结果队列失败");
        }
        
        // 初始化一致性哈希环
        init_consistent_hash_ring();
    }
    
    void start() {
        if (running_) return;
        running_ = true;
        
        // 启动工作器发现线程
        worker_discovery_thread_ = std::thread(&LoadBalancer::worker_discovery_thread_function, this);
        
        // 启动健康检查线程
        health_check_thread_ = std::thread(&LoadBalancer::health_check_thread_function, this);
        
        // 启动负载均衡线程
        load_balancing_thread_ = std::thread(&LoadBalancer::load_balancing_thread_function, this);
        
        std::cout << "负载均衡器启动成功" << std::endl;
    }
    
    void stop() {
        if (!running_) return;
        running_ = false;
        
        if (worker_discovery_thread_.joinable()) {
            worker_discovery_thread_.join();
        }
        if (health_check_thread_.joinable()) {
            health_check_thread_.join();
        }
        if (load_balancing_thread_.joinable()) {
            load_balancing_thread_.join();
        }
        
        if (zk_client_) {
            zk_client_->close();
        }
        
        std::cout << "负载均衡器已停止" << std::endl;
    }
    
    // 设置负载均衡策略
    void set_strategy(LoadBalanceStrategy strategy) {
        strategy_ = strategy;
    }
    
    // 获取负载均衡策略名称
    std::string get_strategy_name() const {
        switch (strategy_) {
            case LoadBalanceStrategy::ROUND_ROBIN: return "轮询";
            case LoadBalanceStrategy::LEAST_CONNECTIONS: return "最少连接数";
            case LoadBalanceStrategy::WEIGHTED_ROUND_ROBIN: return "加权轮询";
            case LoadBalanceStrategy::LEAST_RESPONSE_TIME: return "最快响应时间";
            case LoadBalanceStrategy::CONSISTENT_HASH: return "一致性哈希";
            case LoadBalanceStrategy::RANDOM: return "随机";
            default: return "未知";
        }
    }
    
private:
    // 工作器发现线程
    void worker_discovery_thread_function() {
        while (running_) {
            try {
                // 从Zookeeper获取所有工作器节点
                std::vector<std::string> worker_nodes = zk_client_->getChildren(zk_worker_path_);
                
                std::lock_guard<std::mutex> lock(worker_nodes_mutex_);
                
                // 更新工作器列表
                for (const auto& node_name : worker_nodes) {
                    std::string node_path = zk_worker_path_ + "/" + node_name;
                    std::string node_data = zk_client_->getNodeData(node_path);
                    
                    if (!node_data.empty()) {
                        taskscheduler::WorkerHeartbeat heartbeat;
                        if (heartbeat.ParseFromString(node_data)) {
                            update_worker_node(heartbeat);
                        }
                    }
                }
                
                // 移除不存在的节点
                remove_inactive_workers(worker_nodes);
                
                // 更新一致性哈希环
                update_consistent_hash_ring();
                
            } catch (const std::exception& e) {
                std::cerr << "工作器发现异常: " << e.what() << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    // 健康检查线程
    void health_check_thread_function() {
        while (running_) {
            try {
                std::lock_guard<std::mutex> lock(worker_nodes_mutex_);
                
                time_t current_time = time(nullptr);
                for (auto& worker : worker_nodes_) {
                    // 检查心跳超时
                    if (current_time - worker->last_heartbeat > 30) {
                        worker->is_healthy = false;
                    }
                    
                    // 计算健康分数
                    double health_score = calculate_health_score(worker);
                    if (health_score < 0.3) {
                        worker->is_healthy = false;
                    }
                }
                
            } catch (const std::exception& e) {
                std::cerr << "健康检查异常: " << e.what() << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    
    // 负载均衡线程
    void load_balancing_thread_function() {
        while (running_) {
            try {
                // 从任务队列获取任务并分发
                task_queue_->consumeTask([this](taskscheduler::Task& task) {
                    std::shared_ptr<WorkerNode> selected_worker = select_worker(task);
                    if (selected_worker) {
                        // 分发任务到选中的工作器
                        distribute_task(task, selected_worker);
                    } else {
                        std::cerr << "没有可用的工作器节点" << std::endl;
                        // 可以将任务重新放回队列或记录失败
                    }
                });
                
            } catch (const std::exception& e) {
                std::cerr << "负载均衡异常: " << e.what() << std::endl;
            }
        }
    }
    
    // 选择工作器节点
    std::shared_ptr<WorkerNode> select_worker(const taskscheduler::Task& task) {
        std::lock_guard<std::mutex> lock(worker_nodes_mutex_);
        
        // 过滤出健康的工作器
        std::vector<std::shared_ptr<WorkerNode>> healthy_workers;
        for (const auto& worker : worker_nodes_) {
            if (worker->is_healthy) {
                healthy_workers.push_back(worker);
            }
        }
        
        if (healthy_workers.empty()) {
            return nullptr;
        }
        
        switch (strategy_) {
            case LoadBalanceStrategy::ROUND_ROBIN:
                return select_round_robin(healthy_workers);
            case LoadBalanceStrategy::LEAST_CONNECTIONS:
                return select_least_connections(healthy_workers);
            case LoadBalanceStrategy::WEIGHTED_ROUND_ROBIN:
                return select_weighted_round_robin(healthy_workers);
            case LoadBalanceStrategy::LEAST_RESPONSE_TIME:
                return select_least_response_time(healthy_workers);
            case LoadBalanceStrategy::CONSISTENT_HASH:
                return select_consistent_hash(healthy_workers, task);
            case LoadBalanceStrategy::RANDOM:
                return select_random(healthy_workers);
            default:
                return select_least_connections(healthy_workers);
        }
    }
    
    // 轮询选择
    std::shared_ptr<WorkerNode> select_round_robin(const std::vector<std::shared_ptr<WorkerNode>>& workers) {
        if (workers.empty()) return nullptr;
        
        int index = current_round_robin_index_.fetch_add(1) % workers.size();
        return workers[index];
    }
    
    // 最少连接数选择
    std::shared_ptr<WorkerNode> select_least_connections(const std::vector<std::shared_ptr<WorkerNode>>& workers) {
        if (workers.empty()) return nullptr;
        
        auto min_worker = std::min_element(workers.begin(), workers.end(),
            [](const std::shared_ptr<WorkerNode>& a, const std::shared_ptr<WorkerNode>& b) {
                return a->current_connections < b->current_connections;
            });
        
        return *min_worker;
    }
    
    // 加权轮询选择
    std::shared_ptr<WorkerNode> select_weighted_round_robin(const std::vector<std::shared_ptr<WorkerNode>>& workers) {
        if (workers.empty()) return nullptr;
        
        // 计算总权重
        int total_weight = 0;
        for (const auto& worker : workers) {
            total_weight += worker->weight;
        }
        
        if (total_weight == 0) return workers[0];
        
        // 使用轮询索引选择
        int index = current_round_robin_index_.fetch_add(1) % total_weight;
        
        int current_weight = 0;
        for (const auto& worker : workers) {
            current_weight += worker->weight;
            if (index < current_weight) {
                return worker;
            }
        }
        
        return workers[0];
    }
    
    // 最快响应时间选择
    std::shared_ptr<WorkerNode> select_least_response_time(const std::vector<std::shared_ptr<WorkerNode>>& workers) {
        if (workers.empty()) return nullptr;
        
        auto min_worker = std::min_element(workers.begin(), workers.end(),
            [](const std::shared_ptr<WorkerNode>& a, const std::shared_ptr<WorkerNode>& b) {
                return a->response_time_ms < b->response_time_ms;
            });
        
        return *min_worker;
    }
    
    // 一致性哈希选择
    std::shared_ptr<WorkerNode> select_consistent_hash(const std::vector<std::shared_ptr<WorkerNode>>& workers, 
                                                      const taskscheduler::Task& task) {
        if (workers.empty()) return nullptr;
        
        std::lock_guard<std::mutex> lock(hash_ring_mutex_);
        
        // 使用任务ID计算哈希值
        uint32_t hash = std::hash<std::string>{}(task.task_id());
        
        // 在哈希环中查找下一个节点
        auto it = hash_ring_.lower_bound(hash);
        if (it == hash_ring_.end()) {
            it = hash_ring_.begin();
        }
        
        return it->second;
    }
    
    // 随机选择
    std::shared_ptr<WorkerNode> select_random(const std::vector<std::shared_ptr<WorkerNode>>& workers) {
        if (workers.empty()) return nullptr;
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, workers.size() - 1);
        
        return workers[dis(gen)];
    }
    
    // 分发任务
    void distribute_task(const taskscheduler::Task& task, std::shared_ptr<WorkerNode> worker) {
        try {
            // 更新工作器连接数
            worker->current_connections++;
            
            // 发布任务到对应的工作器队列
            task_queue_->publishTask(task);
            
            std::cout << "任务 " << task.task_id() << " 分发到工作器 " << worker->worker_id 
                      << " (策略: " << get_strategy_name() << ")" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "分发任务失败: " << e.what() << std::endl;
            worker->current_connections--;
        }
    }
    
    // 更新工作器节点信息
    void update_worker_node(const taskscheduler::WorkerHeartbeat& heartbeat) {
        std::string worker_id = heartbeat.worker_id();
        
        // 查找现有节点
        auto it = std::find_if(worker_nodes_.begin(), worker_nodes_.end(),
            [&worker_id](const std::shared_ptr<WorkerNode>& node) {
                return node->worker_id == worker_id;
            });
        
        std::shared_ptr<WorkerNode> worker_node;
        if (it == worker_nodes_.end()) {
            // 创建新节点
            worker_node = std::make_shared<WorkerNode>();
            worker_nodes_.push_back(worker_node);
        } else {
            worker_node = *it;
        }
        
        // 更新节点信息
        worker_node->worker_id = heartbeat.worker_id();
        worker_node->worker_ip = heartbeat.worker_ip();
        worker_node->worker_port = heartbeat.worker_port();
        worker_node->last_heartbeat = heartbeat.timestamp();
        worker_node->is_healthy = heartbeat.is_healthy();
        
        // 更新能力描述
        worker_node->capabilities.clear();
        for (const auto& cap : heartbeat.dec()) {
            worker_node->capabilities[cap.first] = cap.second;
        }
        
        // 更新系统信息
        auto cpu_it = worker_node->capabilities.find("cpu_usage");
        if (cpu_it != worker_node->capabilities.end()) {
            worker_node->cpu_usage = std::stod(cpu_it->second);
        }
        
        auto mem_it = worker_node->capabilities.find("memory_usage");
        if (mem_it != worker_node->capabilities.end()) {
            worker_node->memory_usage = std::stod(mem_it->second);
        }
        
        auto disk_it = worker_node->capabilities.find("disk_usage");
        if (disk_it != worker_node->capabilities.end()) {
            worker_node->disk_usage = std::stod(disk_it->second);
        }
        
        auto net_it = worker_node->capabilities.find("network_speed");
        if (net_it != worker_node->capabilities.end()) {
            worker_node->network_speed = std::stod(net_it->second);
        }
        
        // 计算权重
        worker_node->weight = calculate_worker_weight(worker_node);
    }
    
    // 移除不活跃的工作器
    void remove_inactive_workers(const std::vector<std::string>& active_worker_ids) {
        worker_nodes_.erase(
            std::remove_if(worker_nodes_.begin(), worker_nodes_.end(),
                [&active_worker_ids](const std::shared_ptr<WorkerNode>& worker) {
                    return std::find(active_worker_ids.begin(), active_worker_ids.end(), 
                                   worker->worker_id) == active_worker_ids.end();
                }),
            worker_nodes_.end()
        );
    }
    
    // 计算工作器权重
    int calculate_worker_weight(const std::shared_ptr<WorkerNode>& worker) {
        // 基于CPU、内存、网络等指标计算权重
        double cpu_factor = 1.0 - worker->cpu_usage / 100.0;
        double mem_factor = 1.0 - worker->memory_usage / 100.0;
        double net_factor = std::min(worker->network_speed / 1000.0, 1.0); // 假设1000Mbps为满分
        
        double total_factor = (cpu_factor + mem_factor + net_factor) / 3.0;
        return std::max(1, static_cast<int>(total_factor * 10));
    }
    
    // 计算健康分数
    double calculate_health_score(const std::shared_ptr<WorkerNode>& worker) {
        double score = 1.0;
        
        // CPU使用率影响
        if (worker->cpu_usage > 90) score -= 0.3;
        else if (worker->cpu_usage > 80) score -= 0.2;
        else if (worker->cpu_usage > 70) score -= 0.1;
        
        // 内存使用率影响
        if (worker->memory_usage > 90) score -= 0.3;
        else if (worker->memory_usage > 80) score -= 0.2;
        else if (worker->memory_usage > 70) score -= 0.1;
        
        // 磁盘使用率影响
        if (worker->disk_usage > 90) score -= 0.2;
        else if (worker->disk_usage > 80) score -= 0.1;
        
        // 失败率影响
        if (worker->total_tasks_processed > 0) {
            double failure_rate = static_cast<double>(worker->failed_tasks) / worker->total_tasks_processed;
            if (failure_rate > 0.1) score -= 0.3;
            else if (failure_rate > 0.05) score -= 0.2;
        }
        
        return std::max(0.0, score);
    }
    
    // 初始化一致性哈希环
    void init_consistent_hash_ring() {
        std::lock_guard<std::mutex> lock(hash_ring_mutex_);
        hash_ring_.clear();
    }
    
    // 更新一致性哈希环
    void update_consistent_hash_ring() {
        std::lock_guard<std::mutex> lock(hash_ring_mutex_);
        hash_ring_.clear();
        
        for (const auto& worker : worker_nodes_) {
            if (worker->is_healthy) {
                // 为每个工作器创建多个虚拟节点
                for (int i = 0; i < 150; ++i) {
                    std::string virtual_node = worker->worker_id + "#" + std::to_string(i);
                    uint32_t hash = std::hash<std::string>{}(virtual_node);
                    hash_ring_[hash] = worker;
                }
            }
        }
    }
    
    // 获取负载均衡器统计信息
    Json::Value get_statistics() {
        Json::Value stats;
        std::lock_guard<std::mutex> lock(worker_nodes_mutex_);
        
        stats["strategy"] = get_strategy_name();
        stats["total_workers"] = static_cast<int>(worker_nodes_.size());
        
        int healthy_workers = 0;
        int total_connections = 0;
        double avg_cpu = 0.0;
        double avg_memory = 0.0;
        
        for (const auto& worker : worker_nodes_) {
            if (worker->is_healthy) {
                healthy_workers++;
                total_connections += worker->current_connections;
                avg_cpu += worker->cpu_usage;
                avg_memory += worker->memory_usage;
            }
        }
        
        stats["healthy_workers"] = healthy_workers;
        stats["total_connections"] = total_connections;
        
        if (healthy_workers > 0) {
            stats["avg_cpu_usage"] = avg_cpu / healthy_workers;
            stats["avg_memory_usage"] = avg_memory / healthy_workers;
        }
        
        return stats;
    }
};

// 导出函数声明
extern "C" {
    void* create_load_balancer(const char* zk_host, const char* zk_worker_path,
                               const char* rabbitmq_host, int rabbitmq_port,
                               const char* rabbitmq_user, const char* rabbitmq_password,
                               int task_channel_id, int result_channel_id, int strategy);
    void destroy_load_balancer(void* lb);
    void start_load_balancer(void* lb);
    void stop_load_balancer(void* lb);
    void set_load_balance_strategy(void* lb, int strategy);
    const char* get_load_balancer_stats(void* lb);
}

// 全局负载均衡器实例
static std::map<void*, std::unique_ptr<LoadBalancer>> load_balancers;

// 创建负载均衡器
void* create_load_balancer(const char* zk_host, const char* zk_worker_path,
                           const char* rabbitmq_host, int rabbitmq_port,
                           const char* rabbitmq_user, const char* rabbitmq_password,
                           int task_channel_id, int result_channel_id, int strategy) {
    try {
        LoadBalanceStrategy lb_strategy = static_cast<LoadBalanceStrategy>(strategy);
        
        auto lb = std::make_unique<LoadBalancer>(
            zk_host, zk_worker_path, rabbitmq_host, rabbitmq_port,
            rabbitmq_user, rabbitmq_password, task_channel_id, result_channel_id, lb_strategy
        );
        
        void* handle = lb.get();
        load_balancers[handle] = std::move(lb);
        
        return handle;
    } catch (const std::exception& e) {
        std::cerr << "创建负载均衡器失败: " << e.what() << std::endl;
        return nullptr;
    }
}

// 销毁负载均衡器
void destroy_load_balancer(void* lb) {
    if (lb && load_balancers.count(lb)) {
        load_balancers[lb]->stop();
        load_balancers.erase(lb);
    }
}

// 启动负载均衡器
void start_load_balancer(void* lb) {
    if (lb && load_balancers.count(lb)) {
        load_balancers[lb]->start();
    }
}

// 停止负载均衡器
void stop_load_balancer(void* lb) {
    if (lb && load_balancers.count(lb)) {
        load_balancers[lb]->stop();
    }
}

// 设置负载均衡策略
void set_load_balance_strategy(void* lb, int strategy) {
    if (lb && load_balancers.count(lb)) {
        LoadBalanceStrategy lb_strategy = static_cast<LoadBalanceStrategy>(strategy);
        load_balancers[lb]->set_strategy(lb_strategy);
    }
}

// 获取负载均衡器统计信息
const char* get_load_balancer_stats(void* lb) {
    static std::string result;
    if (lb && load_balancers.count(lb)) {
        Json::Value stats = load_balancers[lb]->get_statistics();
        Json::StreamWriterBuilder writer;
        result = Json::writeString(writer, stats);
        return result.c_str();
    }
    return "{}";
} 