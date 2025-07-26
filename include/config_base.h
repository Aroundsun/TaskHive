#pragma once

#include<string>

//配置文件基类
class ConfigBase
{
public:
    //构造函数
    ConfigBase(const std::string& path):config_file_path_(path){
    }
    //析构函数
    ~ConfigBase() = default;

    //加载配置文件
    virtual void load_config_file() = 0;
    //输出配置信息 
    virtual void print() = 0;
    //获取文件路径
    std::string get_config_file_path() const{
        return config_file_path_;
    }
private:
    std::string config_file_path_;
};