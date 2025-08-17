#pragma once
#include <string>

namespace logsystem
{
    class LogLevel
    {
    public:
        enum class value
        {
            DEBUG, // 调试信息
            INFO,  // 一般信息
            WARN,  // 警告信息
            ERROR, // 错误信息
            FATAL  // 致命错误
        };

        // 提供日志等级的字符串转换接口
        static const char *ToString(value level)
        {
            switch (level)
            {
            case value::DEBUG:
                return "DEBUG";
            case value::INFO:
                return "INFO";
            case value::WARN:
                return "WARN";
            case value::ERROR:
                return "ERROR";
            case value::FATAL:
                return "FATAL";
            default:
                return "UNKNOW";
            }
            return "UNKNOW";
        }
    };
} // namespace logsystem
