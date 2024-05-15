#ifndef TINY_THREAD_POOL_H
#define TINY_THREAD_POOL_H

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>

#define TINY_THREAD_POOL_DEBUG 1

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
    std::queue<std::function<void()>> m_task_queue;
    std::atomic<bool> m_thread_pool_exit_flag;
    void worker()
    {
        while (true)
        {
            std::function<void()> stored_task;
            {
                std::unique_lock<std::mutex> lock(m_lock);
                while (m_task_queue.empty() && !m_thread_pool_exit_flag)
                {
                    m_cv.wait(lock);
                }

                if (m_thread_pool_exit_flag && m_task_queue.empty())
                {
                    m_worker_thread_num--;
                    TTP_DEBUG("worker thread exit, remaining number: %lu\n", m_worker_thread_num);
                    return;
                }
                // start taking task
                stored_task = static_cast<std::function<void()>>(m_task_queue.front());
                m_task_queue.pop();
            }

            try {
                if (stored_task)
                {
                    stored_task();
                }
            }
            catch (const std::exception e) {
                std::cerr << "Failed to execute " << typeid(decltype(stored_task)).name() << ":" << e.what() << std::endl;
            }

            std::unique_lock<std::mutex> lock(m_lock);
            m_busy_worker_thread_num--;
            if (m_worker_thread_num > m_init_thread_num)
            {
                m_worker_thread_num--;
                TTP_DEBUG("worker thread exit, remaining number: %lu\n", m_worker_thread_num);
                return; // destroy this worker thread
            }
        }
    }
    void createWorkerThread()
    {
        m_worker_thread_num++;
        std::thread worker_thread(&TinyThreadPool::worker, this);
        worker_thread.detach();
        TTP_DEBUG("create new worker thread, current worker thread number: %lu\n", m_worker_thread_num);
    }
public:
    template<class Fn, class... Args>
    void addTask(Fn&& fn, Args&&... args)
    {
        std::unique_lock<std::mutex> lock(m_lock);
        auto task = std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...);
        m_task_queue.push(task);
        m_busy_worker_thread_num++;
        if (m_busy_worker_thread_num > m_worker_thread_num && m_worker_thread_num < max_worker_thread_num_limit)
        {
            createWorkerThread();
        }
        m_cv.notify_all();
    }
public:
    TinyThreadPool() : m_init_thread_num(default_init_thread_num), m_worker_thread_num(0), m_busy_worker_thread_num(0), m_thread_pool_exit_flag(false)
    {
        for (size_t i = 0; i < m_init_thread_num; i++)
        {
            createWorkerThread();
        }
    }
    explicit TinyThreadPool(size_t thread_num) : m_init_thread_num(thread_num), m_worker_thread_num(0), m_busy_worker_thread_num(0), m_thread_pool_exit_flag(false)
    {
        if (m_init_thread_num > max_worker_thread_num_limit)
        {
            printf("create TinyThreadPool failed, init thread number must less than or equal to %lu\n", max_worker_thread_num_limit);
            return;
        }
        for (size_t i = 0; i < m_init_thread_num; i++)
        {
            createWorkerThread();
        }
    }
    ~TinyThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(m_lock);
            m_thread_pool_exit_flag = true;
        }
        m_cv.notify_all();
        while (true)
        {
            std::unique_lock<std::mutex> lock(m_lock);
            if (m_worker_thread_num == 0)
            {
                break;
            }
            m_cv.notify_all();
        }
        TTP_DEBUG("bye~\n");
    }

    TinyThreadPool(TinyThreadPool&) = delete;
    TinyThreadPool& operator=(TinyThreadPool&) = delete;

    TinyThreadPool(TinyThreadPool&&) = delete;
    TinyThreadPool& operator=(TinyThreadPool&&) = delete;
};

#endif // !TINY_THREAD_POOL_H
