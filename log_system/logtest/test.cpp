#include "Logger.hpp"
//g++ ./*.cpp -o build/app -std=c++17 -pthread -ljsoncpp -TEST_LOGGER
#if TEST_LOGGER
ThreadPool *tp = nullptr; // 全局线程池指针
logsystem::Config *config; // 全局配置指针 

void test() {
    int cur_size = 0;
    int cnt = 1;
    while (cur_size++ < 2) {
        logsystem::GetLogger("asynclogger")->Info("测试日志-%d", cnt++);
        logsystem::GetLogger("asynclogger")->Warn("测试日志-%d", cnt++);
        logsystem::GetLogger("asynclogger")->Debug("测试日志-%d", cnt++);
        logsystem::GetLogger("asynclogger")->Error("测试日志-%d", cnt++);
        logsystem::GetLogger("asynclogger")->Fatal("测试日志-%d", cnt++);
    }
}

void init_thread_pool() {
    tp = new ThreadPool(config->thread_count);
}
int main() {
    config = logsystem::Config::GetInstance(); // 获取全局配置实例
    init_thread_pool();
    std::shared_ptr<logsystem::LoggerBuilder> Glb(new logsystem::LoggerBuilder());
    Glb->BuildLoggerName("asynclogger");
    Glb->BuildLoggerFlush<logsystem::FileFlush>("./logfile/FileFlush.log");
    Glb->BuildLoggerFlush<logsystem::RollingFileFlush>("./logfile/RollFile_log",1024 * 1024);
   
    logsystem::LoggerManager::GetInstance().AddLogger(Glb->Build());
    test();
    delete(tp);
    return 0;
}
#endif // TEST_LOGGER
