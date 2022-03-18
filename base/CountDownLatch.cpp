#include "CountDownLatch.h"


CountDownLatch::CountDownLatch(int count):_count(count)
{

}

    void CountDownLatch::wait(){
        std::unique_lock<std::mutex> lock(_mutex);
        while (_count>0)
        {
            _cond.wait(lock);
        }
    }

    void CountDownLatch::countDown(){
        std::unique_lock<std::mutex> lock(_mutex);
        --_count;
        if(_count==0){
            _cond.notify_all();
        }
    }

    int CountDownLatch::getCount() const
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _count;
    }