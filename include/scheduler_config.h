#pragma once

#include <string>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <json/json.h>
#include "config_base.h"

class SchedConfig : public ConfigBase {
public:
    static SchedConfig* GetInstance(const std::string& path) {
        static SchedConfig instance(path);
        return &instance;
    }

    // zk 配置
    std::string get_zk_host() const { return zk_host_; }
    std::string get_zk_root_path() const { return zk_root_path_; }
    std::string get_zk_path() const { return zk_path_; }
    std::string get_zk_node() const { return zk_node_; }

    // redis 配置
    std::string get_redis_ip() const { return redis_ip_; }
    int get_redis_port() const { return redis_port_; }
    std::string get_redis_passwd() const { return redis_password_; }

    // rabbitmq 配置
    std::string get_rabbitmq_ip() const { return rabbitmq_ip_; }
    int get_rabbitmq_port() const { return rabbitmq_port_; }
    std::string get_rabbitmq_user() const { return rabbitmq_user_; }
    std::string get_rabbitmq_password() const { return rabbitmq_password_; }
    int get_scheduler_task_channel_id() const { return scheduler_task_channel_id_; }
    int get_scheduler_result_channel_id() const { return scheduler_result_channel_id_; }

    // 能力配置
    std::unordered_map<std::string, std::string> get_dec() const { return dec_; }

    // 心跳时间间隔
    int get_heartbeat_interval() const { return heartbeat_interval_; }

    // 接收任务配置
    std::string get_receive_task_host() const { return receive_task_host_; }
    int get_receive_task_port() const { return receive_task_port_; }

private:
    SchedConfig() = delete;
    explicit SchedConfig(const std::string& configfilepath) : ConfigBase(configfilepath) {
        load_config_file();
    }

    void load_config_file() override {
        std::ifstream ifs(get_config_file_path(), std::ifstream::binary);
        if (!ifs.is_open()) {
            throw std::runtime_error("Failed to open config file: " + get_config_file_path());
        }

        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errs;
        if (!Json::parseFromStream(builder, ifs, &root, &errs)) {
            throw std::runtime_error("Failed to parse config file: " + errs);
        }

        // zk
        zk_host_ = root["zk"]["host"].asString();
        zk_root_path_ = root["zk"]["root_path"].asString();
        zk_path_ = root["zk"]["path"].asString();
        zk_node_ = root["zk"]["node"].asString();

        // redis
        redis_ip_ = root["redis"]["host"].asString();
        redis_port_ = root["redis"]["port"].asInt();
        redis_password_ = root["redis"]["password"].asString();

        // rabbitmq
        rabbitmq_ip_ = root["rabbitmq"]["host"].asString();
        rabbitmq_port_ = root["rabbitmq"]["port"].asInt();
        rabbitmq_user_ = root["rabbitmq"]["user"].asString();
        rabbitmq_password_ = root["rabbitmq"]["password"].asString();
        scheduler_task_channel_id_ = root["rabbitmq"]["task_channel_id"].asInt();
        scheduler_result_channel_id_ = root["rabbitmq"]["result_channel_id"].asInt();

        // capabilities
        const Json::Value& caps = root["capabilities"];
        for (const auto& key : caps.getMemberNames()) {
            dec_[key] = caps[key].asString();
        }

        // heartbeat interval
        heartbeat_interval_ = root["heartbeat_interval"].asInt();

        // receive_task
        receive_task_host_ = root["receive_task"]["host"].asString();
        receive_task_port_ = root["receive_task"]["port"].asInt();
    }

    void print() override {
        std::cout << "======= Scheduler Configuration =======" << std::endl;

        std::cout << "[ZooKeeper]" << std::endl;
        std::cout << "Host       : " << zk_host_ << std::endl;
        std::cout << "Root Path  : " << zk_root_path_ << std::endl;
        std::cout << "Path       : " << zk_path_ << std::endl;
        std::cout << "Node       : " << zk_node_ << std::endl;

        std::cout << "\n[Redis]" << std::endl;
        std::cout << "Host       : " << redis_ip_ << std::endl;
        std::cout << "Port       : " << redis_port_ << std::endl;
        std::cout << "Password   : " << redis_password_ << std::endl;

        std::cout << "\n[RabbitMQ]" << std::endl;
        std::cout << "Host       : " << rabbitmq_ip_ << std::endl;
        std::cout << "Port       : " << rabbitmq_port_ << std::endl;
        std::cout << "User       : " << rabbitmq_user_ << std::endl;
        std::cout << "Password   : " << rabbitmq_password_ << std::endl;
        std::cout << "Task Ch ID : " << scheduler_task_channel_id_ << std::endl;
        std::cout << "Result Ch ID: " << scheduler_result_channel_id_ << std::endl;

        std::cout << "\n[Capabilities]" << std::endl;
        for (const auto& [key, value] : dec_) {
            std::cout << key << " : " << value << std::endl;
        }

        std::cout << "\n[Heartbeat]" << std::endl;
        std::cout << "Interval (ms): " << heartbeat_interval_ << std::endl;

        std::cout << "\n[Receive Task]" << std::endl;
        std::cout << "Host       : " << receive_task_host_ << std::endl;
        std::cout << "Port       : " << receive_task_port_ << std::endl;

        std::cout << "=======================================" << std::endl;
    }

private:
    // zk
    std::string zk_host_;
    std::string zk_root_path_;
    std::string zk_path_;
    std::string zk_node_;

    // redis
    std::string redis_ip_;
    int redis_port_;
    std::string redis_password_;

    // rabbitmq
    std::string rabbitmq_ip_;
    int rabbitmq_port_;
    std::string rabbitmq_user_;
    std::string rabbitmq_password_;
    int scheduler_task_channel_id_;
    int scheduler_result_channel_id_;

    // capabilities
    std::unordered_map<std::string, std::string> dec_;

    // heartbeat
    int heartbeat_interval_;

    // receive_task
    std::string receive_task_host_;
    int receive_task_port_;
};
