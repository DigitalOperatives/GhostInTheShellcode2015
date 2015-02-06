#ifndef CONCURRENT_WORK_QUEUE_H
#define CONCURRENT_WORK_QUEUE_H

#include <condition_variable>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

namespace concurrent {

template <typename Type, typename Queue = std::queue<Type>>
class ConcurrentWorkQueue {
private:
    bool done = false;
    bool joining = false;
    Queue queue;
    struct : std::vector<std::thread> {
        void join() { for_each(begin(), end(), mem_fun_ref(&value_type::join)); }
    } threads;
    std::function<void(const Type&)> statusCallback;
    std::mutex mutex;
    std::mutex callbackMutex;
    std::condition_variable update;

public:
    template<typename Function>
    ConcurrentWorkQueue(Function&& function, std::function<void(const Type&)> statusCallback = nullptr, unsigned numThreads = std::thread::hardware_concurrency()) : statusCallback(statusCallback) {
        if(!numThreads) {
            throw std::invalid_argument("Concurrency must not be zero");
        }

        for(unsigned count = 0; count < numThreads; ++count) {
            threads.emplace_back(static_cast<void(ConcurrentWorkQueue::*)(Function)>(&ConcurrentWorkQueue::consume), this, std::forward<Function>(function));
        }
    }

    ConcurrentWorkQueue(ConcurrentWorkQueue &&) = delete;
    ConcurrentWorkQueue &operator=(ConcurrentWorkQueue &&) = delete;

    template<typename... Args>
    ConcurrentWorkQueue& operator()(Args&&... args) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            /* while(queue.size() == threads.size()) { */
            /*     update.wait(lock); */
            /* } */
            queue.emplace(std::forward<Args>(args)...);
        }
        update.notify_one();
        return *this;
    }

    ~ConcurrentWorkQueue() {
        stop();
    }

    inline void join() {
        std::unique_lock<std::mutex> lock(mutex);
        joining = true;
        lock.unlock();
        threads.join();
        lock.lock();
        done = true;
        lock.unlock();
    }

    void stop() {
        std::unique_lock<std::mutex> lock(mutex);
        if(done) {
            lock.unlock();
        } else {
            done = true;
            lock.unlock();
            update.notify_all();
            join();
        }
    }

private:
    template <typename Function>
    void consume(Function process) {
        std::unique_lock<std::mutex> lock(mutex);
        for(; !done ;) {
            if(!queue.empty()) {
                Type item { std::move(queue.front()) };
                queue.pop();
                lock.unlock();
                if(statusCallback) {
                    std::lock_guard<std::mutex> lock(callbackMutex);
                    statusCallback(item);
                }
                if(!process(item)) {
                    lock.lock();
                    done = true;
                    lock.unlock();
                    update.notify_all();
                }
                lock.lock();
            } else if(joining) {
                break;
            } else {
                update.wait(lock, [this](){ return done || !queue.empty(); });
            }
        }
        lock.unlock();
    }
};

} /* namespace concurrent */

#endif
