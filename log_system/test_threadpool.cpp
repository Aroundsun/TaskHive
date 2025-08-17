

//线程池功能测试
#include"Threadpool.hpp"
#include<iostream>
#include<chrono>

#if TEST_THREADPOOL

void exampleTask(int id)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Task " << id << " completed." << std::endl;
}

int main()
{
    try {
        ThreadPool pool(4); // 创建一个包含4个线程的线程池

        // 提交一些任务到线程池
        for(int i = 0; i < 100; ++i) {
            pool.enqueue(exampleTask, i);
        }

        // 等待一段时间以确保所有任务完成
        std::this_thread::sleep_for(std::chrono::seconds(2));
    } catch(const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
#endif