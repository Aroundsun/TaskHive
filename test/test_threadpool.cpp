// 线程池独立最小测试（不集成业务模块）
#include "threadpool.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <stdexcept>

static int add(int a, int b)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return a + b;
}

static void will_throw()
{
    throw std::runtime_error("task exception");
}

int main()
{
    try
    {
        // FIXED 模式基础功能
        ThreadPool pool_fixed(2, Thread_Mode::FIXED);
        pool_fixed.start();

        std::vector<std::future<int>> futs;
        for (int i = 0; i < 50; ++i)
        {
            futs.emplace_back(pool_fixed.submit(add, i, i));
        }
        int sum = 0;
        for (auto &f : futs) sum += f.get();
        std::cout << "fixed sum=" << sum << std::endl;

        // 异常传播
        auto fex = pool_fixed.submit(will_throw);
        try { fex.get(); } catch (const std::exception &e) { std::cout << e.what() << std::endl; }

        pool_fixed.stop();

        // CACHE 模式：背压与扩容（仅运行，不做严格断言）
        ThreadPool pool_cache(2, Thread_Mode::CACHE, 128, 8);
        pool_cache.start();
        std::vector<std::future<void>> fs;
        for (int i = 0; i < 200; ++i)
        {
            fs.emplace_back(pool_cache.submit([](){ std::this_thread::sleep_for(std::chrono::milliseconds(5)); }));
        }
        for (auto &f : fs) f.get();
        std::cout << "cache size=" << pool_cache.size() << " idle=" << pool_cache.idle() << std::endl;
        pool_cache.stop();

        // 强压测试：高并发提交与大量任务
        {
            const int producers = 8;
            const int per_producer_tasks = 3000; // 共 24000 任务
            const int init_threads = 2;
            const int max_threads = 32;
            ThreadPool pool_stress(init_threads, Thread_Mode::CACHE, 4096, max_threads);
            pool_stress.start();

            std::atomic<int> done{0};
            auto work = [&done]() {
                // 模拟短任务
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                done.fetch_add(1, std::memory_order_relaxed);
            };

            std::vector<std::thread> prod_threads;
            prod_threads.reserve(producers);
            for (int p = 0; p < producers; ++p)
            {
                prod_threads.emplace_back([&pool_stress, &work, per_producer_tasks]() {
                    for (int i = 0; i < per_producer_tasks; ++i)
                    {
                        pool_stress.submit(work);
                    }
                });
            }
            for (auto &t : prod_threads) t.join();

            const int total = producers * per_producer_tasks;
            // 等待全部完成
            while (done.load(std::memory_order_relaxed) < total)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                std::cout << "progress=" << done.load() << "/" << total
                          << " size=" << pool_stress.size() << " idle=" << pool_stress.idle() << std::endl;
            }

            std::cout << "stress finished: done=" << done.load() << "/" << total
                      << " final_size=" << pool_stress.size() << " final_idle=" << pool_stress.idle() << std::endl;
            pool_stress.stop();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "test_threadpool exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}


