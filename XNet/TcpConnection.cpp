#include "TcpConnection.h"

#include <functional>
#include <thread>
#include <sstream>
#include <errno.h>
#include "../base/Platform.h"
#include "../base/AsyncLog.h"
#include "Socket.h"
#include "EventLoop.h"
#include "Channel.h"

using namespace net;

void net::defaultConnectionCallback(const TcpConnectionPtr& conn){
    LOGD("%s->%s is  %s",
    conn->localAddress().toIpPort().c_str(),
    conn->peerAddress().toIpPort().c_str(),
    (conn->connected()?"UP":"DOWN"));
}
void net::defaultConnectionCallback(const TcpConnectionPtr&,ByteBuffer* buf,Timestamp)
{
    buf->retrieveAll();
}

TcpConnection::TcpConnection(EventLoop* loop,const std::string& name,int sockfd,const InetAddress& localAddr,const InetAddress& peerAddr):
m_loop(loop),m_name(name),
m_state(kConnecting),m_socket(new Socket(sockfd)),
m_channel(new Channel(loop,sockfd)),
m_localAddr(localAddr),m_peerAddr(peerAddr),
m_highWaterMark(64*1024*1024)
{
    m_channel->setReadCallback(std::bind(&TcpConnection::handleRead,this,std::placeholders::_1));   
    m_channel->setWriteCallback(std::bind(&TcpConnection::handleWrite,this));
    m_channel->setCloseCallback(std::bind(&TcpConnection::handleClose,this));
    m_channel->setErrorCallback(std::bind(&TcpConnection::handleError,this));

    LOGD("TcpConnection::ctor[%s] at 0x%x fd=%d",m_name.c_str(),this,sockfd);
    m_socket->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOGD("TcpConnection:::dtor[%s] at 0x%x fd=%d state=%s",m_name.c_str(),this,m_channel->fd(),stateToString());
}

void TcpConnection::send(const void* message,int len)
{
    if(m_state==kConnected){
        if(m_loop->isInLoopThread()){
            sendInLoop(message,len);
        }
        else{
            string messages(static_cast<const char*>(message),len);
            m_loop->runInLoop(std::bind(static_cast<void (TcpConnection::*)(const string&)>(&TcpConnection::sendInLoop),this,messages));
        }
    }
}

void TcpConnection::send(const string& message)
{
    if(m_state==kConnected){
        if(m_loop->isInLoopThread()){
            sendInLoop(message);
        }
        else{
            m_loop->runInLoop(std::bind(static_cast<void (TcpConnection::*)(const string&)>(&TcpConnection::sendInLoop),this,message));
        }
    }
}

void TcpConnection::send(ByteBuffer* buf)
{
    if(m_state==kConnected){
        if(m_loop->isInLoopThread()){
            sendInLoop(buf->peek(),buf->readableBytes());
            buf->retrieveAll();
        }
        else{
            m_loop->runInLoop(std::bind(static_cast<void (TcpConnection::*)(const string&)>(&TcpConnection::sendInLoop),this,buf->retrieveAllAsString()));
        }
    }
}

void TcpConnection::shutdown()
{
    if(m_state==kConnected){
        setState(kDisConnecting);
        m_loop->runInLoop(std::bind(&TcpConnection::shutdownInLoop,this));
    }
}

void TcpConnection::shutdownInLoop()
{
    m_loop->assertInLoopThread();
    if(!m_channel->isWriting())
    {
        m_socket->shutdownWrite();
    }
}
        
void TcpConnection::forceClose()
{
    if(m_state==kConnected || m_state==kDisConnecting)
    {
        setState(kDisConnecting);
        m_loop->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop,shared_from_this()));
    }
}

void TcpConnection::setTcpNoDelay(bool on)
{
    m_socket->setTcpNoDelay(on);
}

void TcpConnection::connectEstablished()
{
    m_loop->assertInLoopThread();
    if(m_state!=kConnecting){
        //一定不能走这个分支
        return;
    }

    setState(kConnected);

    //假如正在执行这段代码时，对端关闭了连接
    if (!m_channel->enableReading())
    {
        LOGE("enableReading failed.");
        handleClose();
        return;
    }
    
    m_connectionCallback(shared_from_this());
}

void TcpConnection::connectDestroyed()
{
    m_loop->assertInLoopThread();
    if(m_state==kConnected){
        setState(kDisconnected);
        m_channel->disableAll();

        //是否应该调用m_closeCallback(shared_from_this());?
        m_connectionCallback(shared_from_this());
    }
    m_channel->remove();
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    m_loop->assertInLoopThread();
    int savedErrno=0;
    int32_t n=m_inputBuffer.readFd(m_channel->fd(),&savedErrno);
    if(n>0)
    {
        m_messageCallback(shared_from_this(),&m_inputBuffer,receiveTime);
    }
    else if(n==0)
    {
        handleClose();
    }
    else{
        errno=savedErrno;
        LOGSYSE("TcpConnection::handleRead");
        handleError();
    }
}
void TcpConnection::handleWrite()
{
    m_loop->assertInLoopThread();
    if(m_channel->isWriting())
    {
        int32_t n=sockets::write(m_channel->fd(),m_outputBuffer.peek(),m_outputBuffer.readableBytes());
        if(n>0)
        {
            m_outputBuffer.retrieve(n);
            if(m_outputBuffer.readableBytes()==0)
            {
                m_channel->disableWriting();
                if(m_writeCompleteCallback){
                    m_loop->queueInLoop(std::bind(m_writeCompleteCallback,shared_from_this()));
                }
                if(m_state==kDisConnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else{
            LOGSYSE("TcpConnection::handleWrite");
            handleClose();
        }
    }
    else{
        LOGD("Connection fd=%d is down,no more writing",m_channel->fd());
    }
}
void TcpConnection::handleClose()
{
    //z在linux上一个连接除了问题，会同时触发handleError和handleClose
    //为了避免重复关闭连接，这里判断下当前状态
    //已经关闭了，直接返回
    if(m_state==kDisconnected)
    {
        return;
    }

    m_loop->assertInLoopThread();
    LOGD("fd=%d state=%s",m_channel->fd(),stateToString()));

    setState(kDisconnected);
    m_channel->disableAll();

    TcpConnectionPtr guardThis(shared_from_this());
    m_connectionCallback(guardThis);
    //must be last line
    m_closeCallback(guardThis);
}
void TcpConnection::handleError()
{
    int err=sockets::getSocketError(m_channel->fd());
    LOGE("TcpConnection::%s handleError [%d] -SO_ERROR=%s",m_name.c_str(),strerror(error));

    //调用handleClose关闭连接  回收Channel和fd
    handleClose();
}
void TcpConnection::sendInLoop(const string& message)
{
    sendInLoop(message.c_str(),message.size());
}
void TcpConnection::sendInLoop(const void* message,size_t len)
{
    m_loop->assertInLoopThread();
    int32_t nwrote=0;
    size_t remaining=len;
    bool faultError=false;
    if(m_state==kDisconnected)
    {
        LOGW("disconnected,give up writing");
        return;
    }

    if(!m_channel->isWriting() && m_outputBuffer.readableBytes()==0)
    {
        nwrote=sockets::write(m_channel->fd(),message,len);

        if(nwrote>=0)
        {
            remaining=len-wrote;
            if(remaining==0 && m_writeCompleteCallback)
            {
                m_loop->queueInLoop(std::bind(m_writeCompleteCallback,shared_from_this()));
            }
        }
        else
        {
            nwrote=0;
            if(errno!=EWOULDBLOCK)
            {
                LOGSYSE("TcpConnection::sendInLoop");
                if(errno==EPIPE || errno=ECONNRESET)
                {
                    faultError=true;
                }
            }
        }
    }

    if(remaining>len){
        return;
    }

    if(!faultError && remaining>0)
    {
        size_t oldlen=m_outputBuffer.readableBytes();
        if(oldlen+remaining>=m_highWaterMark 
        && oldlen<m_highWaterMark && m_highWaterMarkCallback)
        {
            m_loop->queueInLoop(std::bind(m_highWaterMarkCallback,shared_from_this(),oldlen+remaining));
        }
        m_outputBuffer.append(static_cast<const char*>(data)+nwrote,remaining);
        if(!m_channel->isWriting())
        {
            m_channel->enableWriting();
        }
    }
}
void TcpConnection::shutdownInLoop()
{
    m_loop->assertInLoopThread();
    if(m_channel->isWriting())
    {
        m_socket->shutdownWrite();
    }
}
void TcpConnection::forceCloseInLoop()
{
    m_loop->assertInLoopThread();
    if(m_state==kConnected || m_state==kDisConnecting)
    {
        handleClose();
    }
}
const char* TcpConnection::stateToString() const
{
    switch (m_state)
    {
    case kDisconnected:
        return "kDisconnected";
    case kConnecting:
        return "kConnecting";
    case kConnected:
        return "kConnected";
    case kDisConnecting:
        return "kDisConnecting";
    default:
        return "unknown state";
    }
}