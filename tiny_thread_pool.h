#ifndef TINY_THREAD_POOL_H
#define TINY_THREAD_POOL_H

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <stack>
#include <functional>

#define TINY_THREAD_POOL_DEBUG 0

#if TINY_THREAD_POOL_DEBUG
#define TTP_DEBUG(fmt, ...) do {printf(fmt, ##__VA_ARGS__);}while(0)
#else
#define TTP_DEBUG(fmt, ...)
#endif

class TinyThreadPool
{
    constexpr static size_t default_init_thread_num = 50;
    constexpr static size_t max_worker_thread_num_limit = 1000;

private:
    size_t m_init_thread_num;
    size_t m_busy_worker_thread_num; // record the worker thread number taking task
    size_t m_worker_thread_num; //  record the total worker thread number

    std::mutex m_lock;
    std::condition_variable m_cv;
    std::stack<std::function<void()>> m_task_stack;
    void worker()
    {
        while (true)
        {
            std::unique_lock<std::mutex> lock(m_lock);
            while (m_task_stack.empty())
            {
                m_cv.wait(lock);
            }

            // start doing task
            auto stored_task = static_cast<std::function<void()>>(m_task_stack.top());
            m_task_stack.pop();
            lock.unlock();

            try {
                if (stored_task)
                {
                    stored_task();
                }
            }
            catch (const std::exception e) {
                std::cerr << "Failed to execute " << typeid(decltype(stored_task)).name() << ":" << e.what() << std::endl;
            }

            lock.lock();
            m_busy_worker_thread_num--;
            if (m_worker_thread_num > m_init_thread_num)
            {
                m_worker_thread_num--;
                TTP_DEBUG("wroker thread exit\n");
                return; // destroy this worker thread
            }
        }
    }
    void createWorkerThread()
    {
        m_worker_thread_num++;
        std::thread worker_thread(&TinyThreadPool::worker, this);
        worker_thread.detach();
        TTP_DEBUG("create new worker thread, current worker thread number: %llu\n", m_worker_thread_num);
    }
public:
    void addTask(std::function<void()> task)
    {
        std::unique_lock<std::mutex> lock(m_lock);
        m_task_stack.push(task);
        m_busy_worker_thread_num++;
        TTP_DEBUG("busy_worker_thread_num: %llu\n", m_busy_worker_thread_num);
        if (m_busy_worker_thread_num > m_worker_thread_num && m_worker_thread_num < max_worker_thread_num_limit)
        {
            createWorkerThread();
        }
        m_cv.notify_all();
    }
public:
    TinyThreadPool() : m_init_thread_num(default_init_thread_num), m_worker_thread_num(0), m_busy_worker_thread_num(0)
    {
        for (size_t i = 0; i < m_init_thread_num; i++)
        {
            createWorkerThread();
        }
    }
    explicit TinyThreadPool(size_t thread_num) : m_init_thread_num(thread_num), m_worker_thread_num(0), m_busy_worker_thread_num(0)
    {
        if (m_init_thread_num > max_worker_thread_num_limit)
        {
            printf("create TinyThreadPool failed, init thread number must less than or equal to %llu\n", max_worker_thread_num_limit);
            return;
        }
        for (size_t i = 0; i < m_init_thread_num; i++)
        {
            createWorkerThread();
        }
    }
    ~TinyThreadPool() {}

    TinyThreadPool(TinyThreadPool&) = delete;
    TinyThreadPool& operator=(TinyThreadPool&) = delete;

    TinyThreadPool(TinyThreadPool&&) = delete;
    TinyThreadPool& operator=(TinyThreadPool&&) = delete;
};

#endif // !TINY_THREAD_POOL_H
