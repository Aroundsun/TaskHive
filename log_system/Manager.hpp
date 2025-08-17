#pragma once
#include <unordered_map>
#include "AsyncLogger.hpp"

namespace logsystem
{
    // 对日志管理器进行管理 懒汉式单例模式
    class LoggerManager
    {
    public:
        static LoggerManager &GetInstance()
        {
            static LoggerManager instance;
            return instance;
        }
        bool LoggerExist(const std::string &name)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            return logger_map_.find(name) != logger_map_.end();
        }

        void AddLogger(AsyncLogger::ptr &&AsyncLogger)
        {
            if (LoggerExist(AsyncLogger->Name()))
                return;
            std::unique_lock<std::mutex> lock(mutex_);
            logger_map_.insert(std::make_pair(AsyncLogger->Name(), std::move(AsyncLogger)));
        }

        AsyncLogger::ptr GetLogger(const std::string &name)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            auto it = logger_map_.find(name);
            if (it == logger_map_.end())
            {
                return AsyncLogger::ptr(); // 如果日志器不存在，返回空指针
            }

            return it->second;
        }
        
        AsyncLogger::ptr DefaultLogger()
        {
            return default_logger_;
        }

    private:
        LoggerManager()
        {
            std::unique_ptr<logsystem::LoggerBuilder> builder(new logsystem::LoggerBuilder());
            builder->BuildLoggerName("default");
            default_logger_ = builder->Build();
            logger_map_.insert(std::make_pair("default", default_logger_));
        }

    private:
        std::mutex mutex_;                                                        // 互斥锁，保护日志器的线程安全
        logsystem::AsyncLogger::ptr default_logger_;                              // 默认日志器
        std::unordered_map<std::string, logsystem::AsyncLogger::ptr> logger_map_; // 存储日志器的映射表
    };

} // namespace logsystem