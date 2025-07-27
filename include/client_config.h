#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <json/json.h>
#include "config_base.h"

class ClientConfig : public ConfigBase {
public:
    static ClientConfig* GetInstance(const std::string& path) {
        static ClientConfig instance(path);
        return &instance;
    }

    // zk
    std::string get_zk_host() const { return zk_host_; }
    std::string get_zk_path() const { return zk_path_; }

    // redis
    std::string get_redis_host() const { return redis_host_; }
    int get_redis_port() const { return redis_port_; }
    std::string get_redis_password() const { return redis_password_; }

    // 更新间隔
    int get_update_interval() const { return update_interval_; }

    // 负载均衡方式
    int get_load_balance_type() const { return load_balance_type_; }
    std::unordered_map<std::string,double> get_load_balance_weight() const { return load_balance_weight_; }

private:
    ClientConfig() = delete;
    explicit ClientConfig(const std::string& path) : ConfigBase(path) {
        load_config_file();
    }

    void load_config_file() override 
    {
        std::ifstream ifs(get_config_file_path(), std::ifstream::binary);
        if (!ifs.is_open()) 
        {
            throw std::runtime_error("Failed to open config file: " + get_config_file_path());
        }

        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errs;
        if (!Json::parseFromStream(builder, ifs, &root, &errs)) 
        {
            throw std::runtime_error("Failed to parse config file: " + errs);
        }

        // zk
        zk_host_ = root["zk"]["host"].asString();
        zk_path_ = root["zk"]["path"].asString();

        // redis
        redis_host_ = root["redis"]["host"].asString();
        redis_port_ = root["redis"]["port"].asInt();
        redis_password_ = root["redis"]["password"].asString();

        // update_interval
        update_interval_ = root["update_interval"].asInt();

        // load_balance_type
        load_balance_type_ = root["load_balance_type"].asInt();

        // load_balance_weight
        for(auto& item : root["load_balance_weight"]) 
        {
            load_balance_weight_[item.asString()] = root["load_balance_weight"][item.asString()].asDouble();
        }
    }

    void print() override {
        std::cout << "====== Client Config ======" << std::endl;

        std::cout << "[ZooKeeper]" << std::endl;
        std::cout << "Host: " << zk_host_ << std::endl;
        std::cout << "Path: " << zk_path_ << std::endl;

        std::cout << "\n[Redis]" << std::endl;
        std::cout << "Host: " << redis_host_ << std::endl;
        std::cout << "Port: " << redis_port_ << std::endl;
        std::cout << "Password: " << redis_password_ << std::endl;

        std::cout << "\n[Update Interval]" << std::endl;
        std::cout << "Interval (ms): " << update_interval_ << std::endl;

        std::cout << "\n[Load Balance Type]" << std::endl;
        std::cout << "Type: " << load_balance_type_ << std::endl;

        std::cout << "\n[Load Balance Weight]" << std::endl;
        for(auto& item : load_balance_weight_) {
            std::cout << "Key: " << item.first << ", Value: " << item.second << std::endl;
        }

        std::cout << "============================" << std::endl;
    }

private:
    std::string zk_host_;
    std::string zk_path_;

    std::string redis_host_;
    int redis_port_;
    std::string redis_password_;

    int update_interval_;

    int load_balance_type_;
    std::unordered_map<std::string,double> load_balance_weight_;
};
