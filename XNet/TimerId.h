#pragma once
#include <stdint.h>

namespace net
{
    class Timer;

    class TimerId
    {
    public:
        TimerId():_timer(nullptr),_sequence(0)
        {

        }
        TimerId(Timer* timer,int64_t seq):_timer(timer),_sequence(seq)
        {

        }
        Timer* getTimer()
        {
            return _timer;
        }
    private:
        Timer* _timer;
        int64_t _sequence;
    } 

}