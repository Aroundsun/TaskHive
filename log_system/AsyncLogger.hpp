#pragma once
#include <atomic>
#include <cassert>
#include <cstdarg>
#include <memory>
#include <mutex>

#include "LogLevel.hpp"
#include "AsyncWorker.hpp"
#include "Message.hpp"
#include "LogFlush.hpp"
#include "CliBackupLog.hpp"
#include "Threadpool.hpp"

extern ThreadPool *tp;

namespace logsystem
{

    // 异步日志记录器
    class AsyncLogger
    {
    public:
        using ptr = std::shared_ptr<AsyncLogger>;
        AsyncLogger(std::string logger_name, std::vector<logsystem::LogFlush::ptr> &flush_list,
                    logsystem::AsyncType async_type = logsystem::AsyncType::BLOCKING_BOUNDED)
            : logger_name_(std::move(logger_name)),
            flush_list_(flush_list.begin(), flush_list.end()),
              async_worker_(logsystem::AsyncWorker(std::bind(&AsyncLogger::RealFlush, this, std::placeholders::_1), async_type))
        {
        }
        ~AsyncLogger() {};
        std::string Name() const { return logger_name_; }

        void Debug(const std::string &file, size_t line, const std::string format, ...)
        {
            // 使用可变参数格式化字符串
            va_list va;
            // 初始化可变参数列表
            va_start(va, format);

            char *ret;
            int r = vasprintf(&ret, format.c_str(), va);
            if (r == -1 )
            {
                perror("vasprintf failed!!!: ");
                return;
            }


            va_end(va);

            serialize(LogLevel::value::DEBUG, file, line, ret);
            
            free(ret);     // 释放动态分配的字符串内存
            ret = nullptr; // 将指针置为 nullptr，避免悬空指针
        }
        void Info(const std::string &file, size_t line, const std::string format, ...)
        {
            va_list va;
            va_start(va, format);
            char *ret;
            int r = vasprintf(&ret, format.c_str(), va);
            if (r == -1 || ret == nullptr)
            {
                perror("vasprintf failed!!!: ");
                return;
            }

            va_end(va);

            serialize(LogLevel::value::INFO, file, line, ret);

            free(ret);
            ret = nullptr;
        }
        void Warn(const std::string &file, size_t line, const std::string format, ...)
        {
            va_list va;
            va_start(va, format);
            char *ret;
            int r = vasprintf(&ret, format.c_str(), va);
            if (r == -1 || ret == nullptr)
            {
                perror("vasprintf failed!!!: ");
                return;
            }

            va_end(va);

            serialize(LogLevel::value::WARN, file, line, ret);
            free(ret);
            ret = nullptr;
        }
        void Error(const std::string &file, size_t line, const std::string format, ...)
        {
            va_list va;
            va_start(va, format);
            char *ret;
            int r = vasprintf(&ret, format.c_str(), va);
            if (r == -1 || ret == nullptr)
            {
                perror("vasprintf failed!!!: ");
                return;
            }

            va_end(va);

            serialize(LogLevel::value::ERROR, file, line, ret);
            free(ret);
            ret = nullptr;
        }
        void Fatal(const std::string &file, size_t line, const std::string format, ...)
        {
            va_list va;
            va_start(va, format);
            char *ret;
            int r = vasprintf(&ret, format.c_str(), va);
            if (r == -1 || ret == nullptr)
            {
                perror("vasprintf failed!!!: ");
                return;
            }
            va_end(va);
            serialize(LogLevel::value::FATAL, file, line, ret);
            free(ret);
            ret = nullptr;
        }

    private:
        // 将日志信息组织起来并写入文件
        void serialize(logsystem::LogLevel::value level, const std::string &file, size_t line, char *ret)
        {
            logsystem::LogMessage msg(level, file, line, logger_name_, ret);
            std::string data = msg.format(); // 格式化日志消息

            if (level == logsystem::LogLevel::value::FATAL ||
                level == logsystem::LogLevel::value::ERROR)
            {
                // 如果是致命错误或错误级别的日志，直接写入备份服务器
                try
                {
                    auto res = tp->enqueue(start_backup, data);
                    res.get(); // 等待任务完成
                }
                catch (const std::runtime_error &e)
                {
                    std::cout << __FILE__ << __LINE__ << "thread pool closed" << std::endl;
                }
            }
            //std::cout << "AsyncLogger::serialize: " << data << std::endl;
            // 否则将日志信息写入异步工作器的缓冲区
            Flush(data.c_str(), data.size()); // 将日志消息推送到异步工作器的缓冲区
            
        }
        // 将日志信息写入异步工作器的缓冲区
        void Flush(const char *data, size_t len)
        {
            async_worker_.Push(data, len); // 将数据推送到异步工作器的缓冲区
        }
        // 异步工作器的回调函数
        void RealFlush(logsystem::Buffer &buffer)
        {
            if (buffer.isEmpty())
            {
                return; // 如果缓冲区为空，直接返回
            }
            for (auto &e : flush_list_)
            {
                std::cout<<flush_list_.size() << " flush_list size" << std::endl;

                e->Flush(buffer.Begin(), buffer.ReadableSize()); // 遍历所有的日志输出方向，写入日志
            }
        }

    private:
        std::string logger_name_;                     // 日志器名称
        std::vector<logsystem::LogFlush::ptr> flush_list_; // 存放各种日志输出方向
        logsystem::AsyncWorker async_worker_;         // 异步工作器，负责日志的异步写入
    };
    // 异步日志构建器
    // 用于构建异步日志记录器的配置
    class LoggerBuilder
    {
    public:
        using ptr = std::shared_ptr<LoggerBuilder>;

        void BuildLoggerName(const std::string &name)
        {
            logger_name_ = name;
        }

        void BuildLopperType(AsyncType type)
        {
            async_type_ = type;
        }

        template <typename FlushType, typename... Args>
        void BuildLoggerFlush(Args &&...args)
        {
            flush_list_.emplace_back(
                LogFlushFactory::CreateLog<FlushType>(std::forward<Args>(args)...));
        }
        //
        AsyncLogger::ptr Build()
        {
            if (logger_name_.empty())
            {
                throw std::runtime_error("Logger name cannot be empty");
                return nullptr;
            }
            // 日志输出方式为空，默认输出到控制台
            if (flush_list_.empty())
            {
                flush_list_.emplace_back(std::make_shared<logsystem::StdoutFlush>());
            }
            return std::make_shared<AsyncLogger>(logger_name_, flush_list_, async_type_);
        }

    private:

        std::string logger_name_;                                                  // 日志器名称
        std::vector<logsystem::LogFlush::ptr> flush_list_;                         // 存放各种日志输出方式
        logsystem::AsyncType async_type_ = logsystem::AsyncType::BLOCKING_BOUNDED; // 异步类型,默认为阻塞有界缓冲区
    };
} // namespace logsystem