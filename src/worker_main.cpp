#include "worker.h"
#include <csignal>
#include <atomic>



int main(int argc, char* argv[]) {
    
    Worker worker;
    worker.start();


    //阻塞等待
    getchar();

    return 0;
}