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
    m_channel->setErrotCallback(std::bind(&TcpConnection::handleError,this));

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
    
}
        
void TcpConnection::forceClose()
{

}

void TcpConnection::setTcpNoDelay(bool on)
{

}