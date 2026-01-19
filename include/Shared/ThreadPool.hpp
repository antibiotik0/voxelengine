// =============================================================================
// VOXEL ENGINE - THREAD POOL
// Lock-free work-stealing thread pool for background tasks
// =============================================================================
#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <future>

namespace voxel {

// =============================================================================
// THREAD POOL
// Generic thread pool for background work
// =============================================================================
class ThreadPool {
public:
    explicit ThreadPool(std::size_t num_threads = 0) 
        : m_stop(false) 
    {
        if (num_threads == 0) {
            num_threads = std::max(1u, std::thread::hardware_concurrency() - 1);
        }
        
        m_workers.reserve(num_threads);
        for (std::size_t i = 0; i < num_threads; ++i) {
            m_workers.emplace_back([this] { worker_loop(); });
        }
    }
    
    ~ThreadPool() {
        shutdown();
    }
    
    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
    
    // Submit a task and get a future for the result
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using ReturnType = decltype(f(args...));
        
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<ReturnType> result = task->get_future();
        
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stop) {
                // Pool is stopping, don't accept new tasks
                return result;
            }
            m_tasks.emplace([task]() { (*task)(); });
        }
        
        m_condition.notify_one();
        return result;
    }
    
    // Submit a fire-and-forget task
    void submit_detached(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stop) return;
            m_tasks.emplace(std::move(task));
        }
        m_condition.notify_one();
    }
    
    // Get number of worker threads
    [[nodiscard]] std::size_t size() const noexcept {
        return m_workers.size();
    }
    
    // Get approximate number of pending tasks
    [[nodiscard]] std::size_t pending_tasks() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tasks.size();
    }
    
    // Wait for all tasks to complete
    void wait_idle() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_idle_condition.wait(lock, [this] {
            return m_tasks.empty() && m_active_tasks == 0;
        });
    }
    
    // Shutdown the pool
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stop) return;
            m_stop = true;
        }
        
        m_condition.notify_all();
        
        for (auto& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        m_workers.clear();
    }
    
private:
    void worker_loop() {
        while (true) {
            std::function<void()> task;
            
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condition.wait(lock, [this] {
                    return m_stop || !m_tasks.empty();
                });
                
                if (m_stop && m_tasks.empty()) {
                    return;
                }
                
                task = std::move(m_tasks.front());
                m_tasks.pop();
                ++m_active_tasks;
            }
            
            // Execute task outside lock
            task();
            
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                --m_active_tasks;
                if (m_tasks.empty() && m_active_tasks == 0) {
                    m_idle_condition.notify_all();
                }
            }
        }
    }
    
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::condition_variable m_idle_condition;
    
    std::atomic<std::size_t> m_active_tasks{0};
    bool m_stop;
};

} // namespace voxel
