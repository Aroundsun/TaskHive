#include "worker.h"

int main(int argc, char* argv[]) {
    Worker worker;
    worker.start();

    //阻塞等待
    std::this_thread::sleep_for(std::chrono::seconds(100));
    return 0;
}