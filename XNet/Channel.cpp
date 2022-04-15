#include "Channel.h"
#include <sstream>

#include "../base/Platform.h"
#include "../base/AsyncLog.h"
#include "Poller.h"
#include "EventLoop.h"

using namespace net;

const int Channel::kNoneEvent=0;
const int Channel::kReadEvent=XPOLLIN | XPOLLPRI;
const int Channel::kWriteEvent=XPOLLOUT;

Channel::Channel(EventLoop* loop,int fd):m_loop(loop),
m_fd(fd),
m_events(0),
m_revents(0),
m_index(-1);
{
}

Channel::~Channel()
{
}

void Channel::handleEvent(Timestamp receiveTime)
{
    /*
    *XPOLLIN，读事件
    XPOLLPRI,读事件，表示紧急数据，例如tcp socket的带外数据
    POLLRDNORM,读事件，表示有普通数据可读
    POLLRDBAND，读事件，表示有优先数据可读
    XPOLLOUT,写事件
    POLLWRNORM，写事件，表示有普通数据可写
    POLLWRBAND，写事件，表示有优先数据可写
    XPOLLRDHUP，stream socket的一端关闭了连接，注意是stream socket，还有raw socket，dgram socket，或者是写段关闭了连接，如果要使用这个事件，
    必须要定义_GNC_SOURCE宏，这个事件可以用来判断链路是否发生异常（通用方法是使用心跳机制），使用这个事件，得包含头文件
    #define _GNU_SOURCE
    #include <poll.h>
    XPOLLERR，仅用于内核设置传出参数revents，表示设备发生错误
    XPOLLHUP，仅用于内核设置传出参数revents，表示设备被挂起，表示这个socket上并没有在网络上建立连接，，比如只调用了socket函数，没有进行connect
    XPOLLNVAL，仅用于内核设置传出参数revents，表示非法请求文件描述符fd没有打开
    */
   LOGD(reventsToString().c_str());
   if((m_revents & XPOLLHUP) && !(m_revents & XPOLLIN)){
       if(m_closeCallback){
           m_closeCallback();
       }
   }

   if(m_revents & XPOLLNVAL){
       LOGW("Channel::handle_event() XPOLLNVAL");
   }

   if(m_revents & (XPOLLERR | XPOLLNVAL)){
       if(m_errorCallback){
           m_errorCallback();
       }
   }

   if(m_revents & (XPOLLIN | XPOLLPRI | XPOLLRDHUP)){
       if(m_readCallback){
           m_readCallback(receiveTime);
       }
   }

   if(m_revents & XPOLLOUT){
       if(m_writeCallback){
           m_writeCallback();
       }
   }
}

bool Channel::enableReading()
{
    m_events |=kReadEvent;
    return update();
}
bool Channel::disableReading()
{
    m_events&=~kReadEvent;
    return update();
}
bool Channel::enableWriting()
{
    m_events|=kWriteEvent;
    return update();
}
bool Channel::disableWriting()
{
    m_events&=~kWriteEvent;
    return update();
}
bool Channel::disableAll()
{
    m_events=kNoneEvent;
    return update();
}

std::string Channel::reventsToString() const
{
    std::ostringstream oss;
    oss<<m_fd<<":";
    if(m_revents& XPOLLIN){
        oss<<"IN ";
    }
    if(m_revents& XPOLLPRI){
        oss<<"PRI ";
    }
    if(m_revents& XPOLLOUT){
        oss<<"OUT ";
    }
    if(m_revents& XPOLLHUP){
        oss<<"HUP ";
    }
    if(m_revents& XPOLLRDHUP){
        oss<<"RDHUP ";
    }
    if(m_revents& XPOLLERR){
        oss<<"ERR ";
    }
    if(m_revents& XPOLLRDHUP){
        oss<<"NVAL ";
    }

    return oss.str().c_str();
}

void Channel::remove()
{
    if(!isNoneEvent()){
        return;
    }
    m_loop->removeChannel(this);
}
    
bool Channel::update()
{
    return m_loop->updateChannel(this);
}