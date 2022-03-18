#pragma once

#include <mutex>
#include <condition_variable>

class CountDownLatch
{
private:
    mutable std::mutex _mutex;
    std::condition_variable _cond;
    int _count;
public:
    explicit CountDownLatch(int count);

    void wait();

    void countDown();

    int getCount() const;
};


