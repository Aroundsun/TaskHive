#pragma once
#include<thread>
#include<vector>
#include<queue>
#include<mutex>
#include<atomic>
#include<condition_variable>
#include<functional>
#include<iostream>
#include<future>

class ThreadPool {

public:
    
    ThreadPool(size_t threads):stop(false)
    {
        if(threads == 0)
            throw std::invalid_argument("Thread count must be greater than zero.");
        for(size_t i = 0; i < threads; ++i)
        {
            workers.emplace_back(
                [this]() {
                    while(true)
                    {
                        std::function<void()> task;
                        {
                            //使用互斥锁保护任务队列
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            //等待条件变量，直到有任务可执行或线程池停止
                            this->condition.wait(
                                lock,[this](){
                                    return this->stop || !this->tasks.empty();
                                }
                            );
                            //如果线程池停止且任务队列为空，则退出线程
                            if(this->stop && this->tasks.empty())
                                return; //如果线程池停止且任务队列为空，则退出线程
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }
                        //执行任务
                        try {
                            task();
                        } catch(const std::exception& e) {
                            std::cerr << "Exception in thread pool worker: " << e.what() << std::endl;
                        } catch(...) {
                            std::cerr << "Unknown exception in thread pool worker." << std::endl;
                        }
                    }
                }
            );
        }
    }
    
    ~ThreadPool()
    {
        {
            //在析构函数中停止线程池
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        //通知所有等待的线程
        condition.notify_all();
        //等待所有线程完成
        // 这里使用了join()来确保所有线程都完成工作
        for (std::thread &worker : workers)
        {
            worker.join();
        }
    }
    //添加任务到线程池
    // 返回一个future对象，用于获取任务的结果
    template<class F,class... Args>
    auto enqueue(F&& task,Args &&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>
    {
        using return_type = typename std::result_of<F(Args...)>::type;

        // 创建一个包装任务的函数
        auto packaged_task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(task), std::forward<Args>(args)...)
        );
        // 获取相关联的future对象
        std::future<return_type> res = packaged_task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if(stop) // 检查线程池是否停止
                throw std::runtime_error("enqueue on stopped ThreadPool");
            // 将任务添加到任务队列
            // 使用std::function<void()>来存储可调用对象
            tasks.emplace(
                [packaged_task](){
                    (*packaged_task)();
                }
            );
        }
        condition.notify_one();
        return res; 
    }

private:
    std::vector<std::thread> workers;  //工作线程
    std::atomic<bool> stop;            //线程池是否停止
    std::queue<std::function<void()>> tasks; //任务队列
    std::mutex queue_mutex;            //任务队列的互斥锁
    std::condition_variable condition;  //条件变量，用于通知线程 

};
