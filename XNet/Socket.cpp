#include "Socket.h"

#include <stdio.h>
#include <string.h>

#include "../base/AsyncLog.h"
#include "../base/Platform.h"
#include "InetAddress.h"
#include "Endian.h"
#include "Callbacks.h"

using namespace net;

Socket::~Socket()
{
    sockets::close(m_sockfd);
}

void Socket::bindAddress(const InetAddress& localaddr)
{
    sockets::bindOrDie(m_sockfd,localaddr.getSockAddrnet());
}

void Socket::listen()
{
    sockets::listenOrDie(m_sockfd);
}

int Socket::accept(InetAddress* peeraddr)
{
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));

    int connfd=sockets::accept(m_sockfd,&addr);
    if(connfd>=0){
        peeraddr->setSockAddrInet(addr);
    }
    return connfd;
}

void Socket::shutdownWrite()
{
    sockets::shutdownWrite(m_sockfd);
}

void Socket::setTcpNoDelay(bool on)
{
    int optval=on?1:0;
    ::setsockopt(m_sockfd,IPPROTO_TCP,TCP_NODELAY,&optval,static_cast<socklen_t>(sizeof optval));
}

void Socket::setReuseAddr(bool on)
{
    sockets::setReuseAddr(m_sockfd,on);
}

void Socket::setReusePort(bool on)
{
    sockets::setReusePort(m_sockfd,on);
}

void Socket::setKeepAlive(bool on)
{
    int optval=on?1:0;
    setsockopt(m_sockfd,SOL_SOCKET,SO_KEEPALIVE,&optval,static_cast<socklen_t>(sizeof optval));
}

SOCKET sockets::createOrDie()
{
    SOCKET sockfd=::socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,IPPROTO_TCP);
    if(sockfd==INVALID_SOCKET)
    {
        LOGF("sockets::createNonblockingOrDie");
    }
    return sockfd;
}
SOCKET sockets::createNonblockingOrDie()
{
    SOCKET sockfd=::socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,IPPROTO_TCP);
    if(sockfd==INVALID_SOCKET)
    {
        LOGF("sockets::createNonblockingOrDie");
    }

    setNonBlockAndCloseOnExec(sockfd);
    return sockfd;
}

//获取文件描述符标志  ----> F_GETFD 获取文件描述符状态标志--->F_GETFL
void sockets::setNonBlockAndCloseOnExec(SOCKET sockfd)
{
    int flags=::fcntl(sockfd,F_GETFL,0);
    flags|=O_NONBLOCK;
    int ret=::fcntl(sockfd,F_SETFL,flags);
    
    flags=::fcntl(sockfd,F_GETFD,0);
    flags|=FD_CLOEXEC;
    ret=::fcntl(sockfd,F_GETFD,flags);
}

void sockets::setReuseAddr(SOCKET sockfd,bool on)
{
    int optval=on?1:0;
    setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&optval,static_cast<socklen_t>(sizeof optval));
}
void sockets::setReusePort(SOCKET sockfd,bool on)
{
    int optval=on?1:0;
    int ret=setsockopt(sockfd,SOL_SOCKET,SO_REUSEPORT,&optval,static_cast<socklen_t>(sizeof optval));
    if(ret<0 && on)
    {
        LOGSYSE("SO_REUSEPORT failed.");
    }
}

SOCKET sockets::connect(SOCKET sockfd,const struct sockaddr_in& addr)
{
    return ::connect(sockfd,sockaddr_cast(&addr),static_cast<socklen_t>(sizeof addr));
}

void sockets::bindOrDie(SOCKET sockfd,const struct sockaddr_in& addr)
{
    int ret=bind(sockfd,sockaddr_cast(&addr),static_cast<socklen_t>(sizeof addr));
    if(ret==SOCKET_ERROR){
        LOGF("sockets::bindOrDie");
    }
}
void sockets::listenOrDie(SOCKET sockfd)
{
    int ret=listen(sockfd,SOMAXCONN);
    if(ret==SOCKET_ERROR){
        LOGF("sockets::listenOrDie");
    }
}
SOCKET sockets::accept(SOCKET sockfd,struct sockaddr_in* addr)
{
    socklen_t addrlen=static_cast<socklen_t>(sizeof *addr);
    SOCKET connfd=accept4(sockfd,sockaddr_cast(addr),&addrlen,SOCK_NONBLOCK|SOCK_CLOEXEC);
    if(connfd==SOCKET_ERROR){
        int savedErrno=errno;
        LOGSYSE("Socket::accept");
        switch (savedErrno)
        {
        case EAGAIN:
        case ECONNABORTED:
        case EINTR:
        case EPROTO:
        case EPERM:
        case EMFILE:
            errno=savedErrno;
            break;
        case EBADF:
        case EFAULT:
        case EINVAL:
        case ENFILE:
        case ENOBUFS:
        case ENOMEM:
        case ENOTSOCK:
        case EOPNOTSUPP:
            LOGF("unexpected error of ::accept %d",savedErrno);
            break;
        default:
            LOGF("unexpected error of ::accept %d",savedErrno);
            break;
        }
    }
    return connfd;
}
int32_t sockets::read(SOCKET sockfd,void* buf,int32_t count)
{
    return ::read(sockfd,buf,count);
}

ssize_t sockets::readv(SOCKET sockfd,const struct iovec* iov,int iovcnt)
{
    return ::readv(sockfd,iov,iovcnt);
}

int32_t sockets::write(SOCKET sockfd,const void* buf,int32_t count)
{
    return ::write(sockfd,buf,count);
}

void sockets::close(SOCKET sockfd)
{
    if(::close(sockfd)<0)
    {
        LOGSYSE("sockfd::close,fd=%d,errno=%d,errorinfo=%s",sockfd,errno,strerror(errno));
    }
}
void sockets::shutdownWrite(SOCKET sockfd)
{
    if(shutdown(sockfd,SHUT_WR)<0)
    {
        LOGSYSE("sockets::shutdownWrite");
    }
}

//新型网路地址转化函数inet_pton和inet_ntop   函数中p和n分别代表presentation和numberic
void sockets::toIpPort(char* buf,size_t size,const struct sockaddr_in& addr)
{
    inet_ntop(AF_INET,&addr.sin_addr,buf,static_cast<socklen_t>(size));
    size_t end=strlen(buf);
    uint16_t port=sockets::networkToHost16(addr.sin_port);
    //inet_ntop已经将ip填充，追加端口号
    snprintf(buf+end,size-end,":%u",port); 
}

void sockets::toIp(char* buf,size_t size,const struct sockaddr_in& addr)
{
    //不知道为啥需要这个判断
    if(size>=sizeof(struct sockaddr_in)){
        return;
    }

    inet_ntop(AF_INET,&addr.sin_addr,buf,static_cast<socklen_t>(size));
}

void sockets::fromIpPort(const char* ip,uint16_t port,struct sockaddr_in* addr)
{
    //主机序转换为网络序
    addr->sin_family=AF_INET;
    addr->sin_port=sockets::hostToNetwork16(port);
    if(inet_pton(AF_INET,ip,&addr->sin_addr)<=0)
    {
        //用于技术框架本身的错误
        LOGSYSE("sockets::fromIpPort");
    }
}

int sockets::getSocketError(SOCKET sockfd)
{
    int optval;
    socklen_t optlen=static_cast<socklen_t>(sizeof optval);

    if(getsockopt(sockfd,SOL_SOCKET,SO_ERROR,&optval,&optlen)<0)
    {
        return errno;
    }
    return optval;
}

const struct sockaddr* sockets::sockaddr_cast(const struct sockaddr_in* addr)
{
    return static_cast<const struct sockaddr*>((void*)addr);
}
struct sockaddr* sockets::sockaddr_cast(struct sockaddr_in* addr)
{
    return static_cast<struct sockaddr*>((void*)addr);
}
const struct sockaddr_in* sockets::sockaddr_in_cast(const struct sockaddr* addr)
{
    return static_cast<const struct sockaddr_in*>((void*)addr);
}
struct sockaddr_in* sockaddr_in_cast(struct sockaddr* addr)
{
    return static_cast<struct sockaddr_in*>((void*)addr);
}

struct sockaddr_in sockets::getLocalAddr(SOCKET sockfd)
{
    struct sockaddr_in localaddr={0};
    memset(&localaddr,0,sizeof(localaddr));
    socklen_t addrlen=static_cast<socklen_t>(sizeof localaddr);
    getsockname(sockfd,sockaddr_cast(&localaddr),&addrlen);
    return localaddr;
}
struct sockaddr_in sockets::getPeerAddr(SOCKET sockfd)
{
    struct sockaddr_in peeraddr={0};
    memset(&peeraddr,0,sizeof(peeraddr));
    socklen_t addrlen=static_cast<socklen_t>(sizeof peeraddr);
    getpeername(sockfd,sockaddr_cast(&peeraddr),&addrlen);
    return peeraddr;
}

bool sockets::isSelfConnect(SOCKET sockfd)
{
    struct sockaddr_in localaddr=getLocalAddr(sockfd);
    struct sockaddr_in peeraddr=getLocalAddr(sockfd);
    return localaddr.sin_addr.s_addr==peeraddr.sin_addr.s_addr && localaddr.sin_port==peeraddr.sin_port;
}