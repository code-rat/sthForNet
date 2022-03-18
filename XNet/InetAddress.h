#pragma once

#include <string>
#include "../base/Platform.h"

namespace net{
    class InetAddress
    {
        public:
            explicit InetAddress(uint16_t port=0,bool loopbackOnly=false);

            InetAddress(const std::string &ip,uint16_t port);

            InetAddress(const struct sockaddr_in& addr):_maddr(addr){}

            std::string toIp()const;
            std::string toIpPort()const;
            uint16_t toPort()const;

            const struct sockaddr_in& getSockAddrnet() const {return _maddr;}
            void setSockAddrInet(const struct sockaddr_in& addr){_maddr=addr;}

            uint32_t ipNetEndian() const{return _maddr.sin_addr.s_addr;}
            uint16_t portNetEndian()const{return _maddr.sin_port;}

            static bool resolve(const std::string& hostname,InetAddress* result);
        private:
            struct  sockaddr_in _maddr;
            
    };
}
