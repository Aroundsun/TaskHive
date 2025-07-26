#pragma once

#include <string>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include "json/json.h"
#include "config_base.h"

class WorkerConfig : public ConfigBase {
public:
    static WorkerConfig* GetInstance(const std::string& path) {
        static WorkerConfig instance(path);
        return &instance;
    }
    //工作器配置
    std::string get_worker_ip()const{return worker_ip_;}
    int get_worker_port()const { return worker_port_;}
    std::string get_worker_id(){return worker_id_;}
    // ZooKeeper 配置
    std::string get_zk_host() const { return zk_host_; }
    std::string get_zk_root_path() const { return zk_root_path_; }
    std::string get_zk_path() const { return zk_path_; }
    std::string get_zk_node() const { return zk_node_; }

    // RabbitMQ 配置
    std::string get_rabbitmq_ip() const { return rabbitmq_ip_; }
    int get_rabbitmq_port() const { return rabbitmq_port_; }
    std::string get_rabbitmq_user() const { return rabbitmq_user_; }
    std::string get_rabbitmq_password() const { return rabbitmq_password_; }
    int get_task_channel_id() const { return task_channel_id_; }
    int get_result_channel_id() const { return result_channel_id_; }

    // 能力信息
    std::unordered_map<std::string, std::string> get_capabilities() const { return capabilities_; }

    // 心跳间隔
    int get_heartbeat_interval() const { return heartbeat_interval_; }

private:
    WorkerConfig() = delete;
    explicit WorkerConfig(const std::string& filepath) : ConfigBase(filepath) {
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
        //worker
        worker_ip_ = root["worker"]["worker_ip"].asString();
        worker_port_ = root["worker"]["worker_port"].asInt();
        worker_id_ = root["worker"]["worker_id"].asString();
        // zk
        zk_host_ = root["zk"]["host"].asString();
        zk_root_path_ = root["zk"]["root_path"].asString();
        zk_path_ = root["zk"]["path"].asString();
        zk_node_ = root["zk"]["node"].asString();

        // rabbitmq
        rabbitmq_ip_ = root["rabbitmq"]["host"].asString();
        rabbitmq_port_ = root["rabbitmq"]["port"].asInt();
        rabbitmq_user_ = root["rabbitmq"]["user"].asString();
        rabbitmq_password_ = root["rabbitmq"]["password"].asString();
        task_channel_id_ = root["rabbitmq"]["task_channel_id"].asInt();
        result_channel_id_ = root["rabbitmq"]["result_channel_id"].asInt();

        // capabilities
        const Json::Value& caps = root["capabilities"];
        for (const auto& key : caps.getMemberNames()) {
            capabilities_[key] = caps[key].asString();
        }

        // heartbeat
        heartbeat_interval_ = root["heartbeat_interval"].asInt();
    }

    void print() override {
        std::cout << "====== Worker Config ======" << std::endl;

        std::cout << "[ZooKeeper]" << std::endl;
        std::cout << "Host: " << zk_host_ << std::endl;
        std::cout << "Root Path: " << zk_root_path_ << std::endl;
        std::cout << "Path: " << zk_path_ << std::endl;
        std::cout << "Node: " << zk_node_ << std::endl;

        std::cout << "\n[RabbitMQ]" << std::endl;
        std::cout << "Host: " << rabbitmq_ip_ << std::endl;
        std::cout << "Port: " << rabbitmq_port_ << std::endl;
        std::cout << "User: " << rabbitmq_user_ << std::endl;
        std::cout << "Password: " << rabbitmq_password_ << std::endl;
        std::cout << "Task Channel ID: " << task_channel_id_ << std::endl;
        std::cout << "Result Channel ID: " << result_channel_id_ << std::endl;

        std::cout << "\n[Capabilities]" << std::endl;
        for (const auto& [key, value] : capabilities_) {
            std::cout << key << ": " << value << std::endl;
        }

        std::cout << "\n[Heartbeat]" << std::endl;
        std::cout << "Interval (ms): " << heartbeat_interval_ << std::endl;
        std::cout << "worker_ip: "<<worker_ip_ <<std::endl;
        std::cout << "worker_port: "<<worker_port_ <<std::endl;
        std::cout << "worker_id: "<<worker_id_ <<std::endl;
        
        std::cout << "============================" << std::endl;
    }

private:
    //worker 配置
    std::string worker_ip_;
    int worker_port_;
    std::string worker_id_;
    // zk 配置
    std::string zk_host_;
    std::string zk_root_path_;
    std::string zk_path_;
    std::string zk_node_;

    // RabbitMQ 配置
    std::string rabbitmq_ip_;
    int rabbitmq_port_;
    std::string rabbitmq_user_;
    std::string rabbitmq_password_;
    int task_channel_id_;
    int result_channel_id_;

    // 能力信息
    std::unordered_map<std::string, std::string> capabilities_;

    // 心跳间隔
    int heartbeat_interval_;
};
