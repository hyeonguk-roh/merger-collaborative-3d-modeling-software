#pragma once
#include <mutex>
#include <queue>

template <typename T>
class LockQueue
{
    private:
        std::mutex mutex;
        std::queue<T> queue;

    public:
        void push(const T& t)
        {
            std::scoped_lock lock(mutex);
            queue.push(t);
        }

        T pop()
        {
            std::scoped_lock lock(mutex);
            if (queue.empty()) return T{};
            T t = queue.front();
            queue.pop();
            return t;
        }

        bool empty()
        {
            std::scoped_lock lock(mutex);
            return queue.empty();
        }
};