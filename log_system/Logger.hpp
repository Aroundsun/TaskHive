#pragma once

#include "Manager.hpp"

namespace logsystem
{
    // 用户获取日志器
    logsystem::AsyncLogger::ptr GetLogger(const std::string &name)
    {
        return logsystem::LoggerManager::GetInstance().GetLogger(name);
    }
    // 获取默认日志器
    logsystem::AsyncLogger::ptr DefaultLogger()
    {
        return logsystem::LoggerManager::GetInstance().DefaultLogger();
    }

    // 简化用户使用，宏函数默认填上文件吗+行号
#define Debug(fmt, ...) Debug(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Info(fmt, ...) Info(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Warn(fmt, ...) Warn(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Error(fmt, ...) Error(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Fatal(fmt, ...) Fatal(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

// 无需获取日志器，默认标准输出
#define LOGDEBUGDEFAULT(fmt, ...) logsystem::DefaultLogger()->Debug(fmt, ##__VA_ARGS__)
#define LOGINFODEFAULT(fmt, ...) logsystem::DefaultLogger()->Info(fmt, ##__VA_ARGS__)
#define LOGWARNDEFAULT(fmt, ...) logsystem::DefaultLogger()->Warn(fmt, ##__VA_ARGS__)
#define LOGERRORDEFAULT(fmt, ...) logsystem::DefaultLogger()->Error(fmt, ##__VA_ARGS__)
#define LOGFATALDEFAULT(fmt, ...) logsystem::DefaultLogger()->Fatal(fmt, ##__VA_ARGS__)

} // namespace logsystem