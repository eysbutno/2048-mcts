#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <future>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <stop_token>
#include <type_traits>

class thread_pool {
public:
    explicit thread_pool(std::size_t thread_count = std::thread::hardware_concurrency()) {
        if (thread_count == 0) thread_count = 1;

        for (std::size_t i = 0; i < thread_count; ++i) {
            workers.emplace_back([this](std::stop_token st) {
                worker_loop(st);
            });
        }
    }

    ~thread_pool() {
        shutdown();
    }

    thread_pool(const thread_pool&) = delete;
    thread_pool& operator=(const thread_pool&) = delete;

    /**
     * Submit a task to the pool.
     * Works with lambdas, functions, functors, member functions, etc.
     */
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> result = task->get_future();

        {
            std::lock_guard lock(queue_mutex);
            if (stopped)
                throw std::runtime_error("thread_pool is stopped");

            tasks.emplace([task]() {
                (*task)();
            });
        }

        cv.notify_one();
        return result;
    }

    /**
     * Explicit shutdown (optional, destructor already does this)
     */
    void shutdown() {
        {
            std::lock_guard lock(queue_mutex);
            if (stopped) return;
            stopped = true;
        }

        cv.notify_all();
        workers.clear(); // jthread destructor requests stop + joins
    }

private:
    void worker_loop(std::stop_token st) {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock lock(queue_mutex);

                cv.wait(lock, [&] {
                    return stopped || !tasks.empty() || st.stop_requested();
                });

                if ((stopped || st.stop_requested()) && tasks.empty())
                    return;

                task = std::move(tasks.front());
                tasks.pop();
            }

            task();
        }
    }

private:
    std::vector<std::jthread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable cv;

    bool stopped = false;
};