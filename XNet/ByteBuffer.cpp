#include "ByteBuffer.h"

#include "../base/Platform.h"
#include "Socket.h"
#include "Callbacks.h"

using namespace net;

const char ByteBuffer::kCRLF[]="\r\n";

const size_t ByteBuffer::kCheapPrepend;
const size_t ByteBuffer::kInitialSize;


//分散读
int32_t ByteBuffer::readFd(int fd,int* savedErrno)
{
    char extrabuf[65536];
    const size_t writable=writableBytes();

    //linux
    struct iovec vec[2];

    vec[0].iov_base=begin()+m_writerIndex;
    vec[0].iov_len=writable;
    vec[1].iov_base=extrabuf;
    vec[1].iov_len=sizeof extrabuf;

    const int iovcnt=(writable<sizeof extrabuf)?2:1;
    const ssize_t n=sockets::readv(fd,vec,iovcnt);

    if(n<=0)
    {
        *savedErrno=errno;
    }
    else if(size_t(n)<writable)
    {
        m_writerIndex+=n;
    }
    else{
        //linux平台
        m_writerIndex=m_buffer.size();
        append(extrabuf,n-writable);
    }

    return n;
}