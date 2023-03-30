#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/**
 * 从 fd 上读取数据（Poller 工作在 LT 模式）
 * Buffer 缓冲区是有大小的，但是从 fd 上读数据时却不知道 tcp 数据最终的大小
*/
ssize_t Buffer::readFd(int fd, int *saveErrno) {
    char extrabuf[65536] = {0};     // 栈上内存空间 64K

    struct iovec vec[2];

    const size_t writable = writeableBytes();   // Buffer 缓冲区剩余可写大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if(n < 0) {
        *saveErrno = errno;
    } else if(n <= writable) {      // Buffer 足够存储要读的数据
        writerIndex_ += n;
    } else {    // extrabuf 中也写入了数据
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);     // 把 extrabuf 中的数据写到 Buffer 缓冲区中
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int *saveErrno) {
    ssize_t n = ::write(fd, peek(), readableBytes());
    if(n < 0) {
        *saveErrno = errno;
    }
    return n;
}