#pragma once

#include <stdint.h>
#include "../base/Platform.h"

//struct tcp_info is in <netinet/tcp.h>
struct tcp_info;

namespace net
{
    class InetAddress;

    class Socket
    {
    public:
        explicit Socket(int sockfd):m_sockfd(sockfd){

        }

        ~Socket();

        SOCKET fd()const{ return m_sockfd;}

        void bindAddress(const InetAddress& localaddr);
        void listen();

        int accept(InetAddress* peeraddr);

        void shutdownWrite();
        void setTcpNoDelay(bool on);
        void setReuseAddr(bool on);
        void setReusePort(bool on);
        void setKeepAlive(bool on);
    private:
        const SOCKET m_sockfd;
    };


//sockaddr与socketaddr_in的区别：
//1.两者结构相同，但是sockaddr是交给操作系统使用的，sockaddr_in是给程序员使用的，它区分了地址和端口，使用更方便
//2.结构
/*
struct sockaddr {  
    unsigned short    sa_family;    // 2 bytes address family, AF_xxx  unsiged short
    char              sa_data[14];     // 14 bytes of protocol address  
};

struct sockaddr_in {  
    short            sin_family;       // 2 bytes e.g. AF_INET, AF_INET6  
    unsigned short   sin_port;    // 2 bytes e.g. htons(3490)  
    struct in_addr   sin_addr;     // 4 bytes see struct in_addr, below  
    char             sin_zero[8];     // 8 bytes zero this if you want to  
};  

struct in_addr {  
    unsigned long s_addr;          // 4 bytes load with inet_pton()  
};
一般的用法为：程序员把类型、ip地址、端口填充sockaddr_in结构体，然后强制转换成sockaddr，作为参数传递给系统调用函数
*/

namespace sockets{
    SOCKET createOrDie();
    SOCKET createNonblockingOrDie();

    void setNonBlockAndCloseOnExec(SOCKET sockfd);

    void setReuseAddr(SOCKET sockfd,bool on);
    void setReusePort(SOCKET sockfd,bool on);

    SOCKET connect(SOCKET sockfd,const struct sockaddr_in& addr);
    void bindOrDie(SOCKET sockfd,const struct sockaddr_in& addr);
    void listenOrDie(SOCKET sockfd);
    SOCKET accept(SOCKET sockfd,struct sockaddr_in* addr);
    int32_t read(SOCKET sockfd,void* buf,int32_t count);

    ssize_t readv(SOCKET sockfd,const struct iovec* iov,int iovcnt);

    int32_t write(SOCKET sockfd,const void* buf,int32_t count);

    void close(SOCKET sockfd);
    void shutdownWrite(SOCKET sockfd);

    void toIpPort(char* buf,size_t size,const struct sockaddr_in& addr);
    void toIp(char* buf,size_t size,const struct sockaddr_in& addr);
    void fromIpPort(const char* ip,uint16_t port,struct sockaddr_in* addr);

    int getSocketError(SOCKET sockfd);

    const struct sockaddr* sockaddr_cast(const struct sockaddr_in* addr);
    struct sockaddr* sockaddr_cast(struct sockaddr_in* addr);
    const struct sockaddr_in* sockaddr_in_cast(const struct sockaddr* addr);
    struct sockaddr_in* sockaddr_in_cast(struct sockaddr* addr);

    struct sockaddr_in getLocalAddr(SOCKET sockfd);
    struct sockaddr_in getPeerAddr(SOCKET sockfd);
    bool isSelfConnect(SOCKET sockfd);
}
}