#include "scheduler.h"
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <iostream>

// 全局原子变量控制程序运行状态
template<typename T>
using atomic_t = std::atomic<T>;
atomic_t<bool> g_running(false);

// 信号处理函数
void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        std::cout << "接收到终止信号，正在关闭调度器..." << std::endl;
        g_running = false;
    }
}

int main(int argc, char* argv[]) {
    // 注册信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    scheduler scheduler;
    scheduler.start();
    g_running = true;

    // 可中断的循环等待
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    return 0;
}