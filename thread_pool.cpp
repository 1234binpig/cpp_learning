#include "thread_pool.h"

ThreadPool::ThreadPool(size_t thread_num){
    for(int i=0;i<thread_num;i++){
        workers.emplace_back([this](){
            while(1){
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    //判断任务队列是否为空以及线程池是否关闭
                    condition.wait(lock,[this](){
                    return stop_pool || tasks.empty();
                    });

                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        }
        );
    }
}
void ThreadPool::add_task(std::function<void()> task){
    {
        std::unique_lock<std::mutex> lock(_mutex);
        tasks.push(std::move(task));
    }
    condition.notify_one();
}
ThreadPool::~ThreadPool(){
    {
        std::unique_lock<std::mutex> lock(_mutex);
        stop_pool = true;
    }
    for(auto& worker:workers){
        worker.join();
    }
}