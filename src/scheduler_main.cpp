#include "scheduler.h"
#include <thread>
#include <chrono>
int main(int argc, char* argv[]) {
    scheduler scheduler;
    scheduler.start();

    //阻塞等待
    std::this_thread::sleep_for(std::chrono::seconds(100));

    return 0;
}