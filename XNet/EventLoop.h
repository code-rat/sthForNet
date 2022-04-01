#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "../base/Timestamp.h"
#include "../base/Platform.h"
#include "Callbacks.h"
#include "Socket.h"
#include "TimerId.h"
#include "TimerQueue.h"

namespace net
{
    class EventLoop;
    class Channel;
    class Poller;
    class CTimerHeap;

    class EventLoop
    {
    public:
        typedef std::function<void()> Functor;

    private:
        typedef std::vector<Channel*> ChannelList;

        bool m_looping;
        bool m_quit;
        bool m_eventHandling;
        bool m_doingOtherTasks;
        const std::thread::id m_threadId;
        Timestamp m_pollReturnTime;
        std::unique_ptr<Poller> m_poller;
        std::unique_ptr<TimerQueue> m_timerQueue;
        int64_t m_iteration;

        SOCKET m_wakeupFd;

        std::unique_ptr<Channel> m_wakeupChannel;

        ChannelList m_activeChannels;
        Channel* currentActiveChannel_;

        std::mutex m_mutex;
        std::vector<Functor> m_pendingFunctors;

        Functor m_frameFunctor;
    }
}