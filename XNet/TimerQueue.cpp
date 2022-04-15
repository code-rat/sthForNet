#include "TimerQueue.h"

#include <functional>

#include "../base/Platform.h"
#include "../base/AsyncLog.h"

#include "EventLoop.h"
#include "Timer.h"
#include "TimerId.h"

namespace net
{

}

using namespace net;

TimerQueue::TimerQueue(EventLoop* loop):m_loop(loop),m_timers()
{

}

TimerQueue::~TimerQueue()
{
    for(TimerList::iterator it=m_timers.begin();;it!=m_timers.end();++it)
    {
        delete it->second;
    }
}

//interval单位是微秒
TimerId TimerQueue::addTimer(const TimerCallback& cb,Timestamp when,int64_t interval,int64_t repeatCount)
{
    /*****
     * 原来这里没有加上repeatCount的参数
    */
    Timer *timer=new Timer(cb,when,interval,repeatCount);
    m_loop->runInLoop(std::bind(&TimerQueue::addTimerInLoop,this,timer));
    return TimerId(timer,timer->sequence());
}

TimerId TimerQueue::addTimer(const TimerCallback&& cb,Timestamp when,int64_t interval,int64_t repeatCount)
{
    Timer *timer=new Timer(std::move(cb),when,interval,repeatCount);
    m_loop->runInLoop(std::bind(&TimerQueue::addTimerInLoop,this,timer));
    return TimerId(timer,timer->sequence());
}

void TimerQueue::removeTimer(TimerId timerId)
{
    m_loop->runInLoop(std::bind(&TimerQueue::removeTimerInLoop,this,timerId));
}

void TimerQueue::cancel(TimerId timerId,bool off)
{
    m_loop->runInLoop(std::bind(&TimerQueue::cancelTimerInLoop,this,timerId,off));
}

//called when timerfd alarms
void TimerQueue::doTimer()
{
    m_loop->assertInLoopThread();

    Timestamp now(Timestamp::now());

    for(auto iter=m_timers.begin();iter!=m_timers.end();)
    {
        if(iter->second->expiration()<now)
        {
            iter->second->run();
            if(iter->second->getRepeatCount()==0)
            {
                iter=m_timers.erase(iter);
            }
            else{
                iter++;
            }
        }
        else{
            break;
        }
    }
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
    m_loop->assertInLoopThread();
    insert(timer);
}

void TimerQueue::removeTimerInLoop(TimerId timerId)
{
    m_loop->assertInLoopThread();

    Timer* timer=timerId._timer;
    for(auto iter=m_timers.begin();iter!=m_timers.end();++iter)
    {
        if(iter->second==timer){
            m_timers.erase(iter);
            break;
        }
    }
}

void TimerQueue::cancelTimerInLoop(TimerId timerId,bool off)
{
    m_loop->assertInLoopThread();
    
    Timer* timer=timerId._timer;
    for(auto iter=m_timers.begin();iter!=m_timers.end();++iter)
    {
        if(iter->second==timer){
            iter->second->cancel(off);
            break;
        }
    }
}

void TimerQueue::insert(Timer* timer)
{
    m_loop->assertInLoopThread();
    bool eraliestChanged=false;
    Timestamp when=timer->expiration();
    m_timers.insert(Entry(when,timer));
}