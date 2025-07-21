#include "scheduler.h"
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <iostream>

//信号处理函数
int main(int argc, char* argv[]) {
    //注册信号处理函数
    

    scheduler scheduler;
    scheduler.start();

    //阻塞等待
    getchar();


    
    return 0;
}