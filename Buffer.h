#ifndef _BUFFER_H
#define _BUFFER_H

#include <vector>
#include <string>
#include <algorithm>
#include <arpa/inet.h>
#include <string.h>

namespace mymuduo {

// 网络库底层的缓冲区类型
class Buffer {
public:
    // 默认空闲区域大小
    static const size_t kCheapPrepend = 8;
    // 可读 + 可写区域大小
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initalSize = kInitialSize)
        : buffer_(kCheapPrepend + initalSize),
          readerIndex_(kCheapPrepend),
          writerIndex_(kCheapPrepend)
    {}

    size_t readableBytes() const {
        return writerIndex_ - readerIndex_;
    }

    size_t writeableBytes() const {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const {
        return readerIndex_;
    }

    // 返回缓冲区中可读数据的起始地址
    const char *peek() const {
        return begin() + readerIndex_;
    }

    int32_t peekInt32() const {
        if(readableBytes() >= sizeof(int32_t)) {
            int32_t nw32 = 0;
            ::memcpy(&nw32, peek(), sizeof(nw32));
            int32_t host32 = ntohl(nw32);
            return host32;
        }
    }

    void hasWritten(size_t len) {
        if(len <= writeableBytes()) {
            writerIndex_ += len;
        }
    }

    void retrieve(size_t len) {
        if(len < readableBytes()) {
            // 应用只读取了可读缓冲区中的一部分数据(即 len)，还剩下 readerIndex_ + len ---> writeIndex_
            readerIndex_ += len;
        } else {    // len == readableBytes()
            retrieveAll();
        }
    }

    void retrieveAll() {
        readerIndex_  = writerIndex_ = kCheapPrepend;
    }

    // 把 onMessage 函数上报的 Buffer 数据，转成 string 类型的数据返回
    std::string retrieveAllAsString() {
        // 应用可读取数据的长度
        return retrieveAsString(readableBytes());
    }

    std::string retrieveAsString(size_t len) {
        std::string result(peek(), len);
        retrieve(len);      // 上面一句把缓冲区中可读的数据已经读取出来，这里肯定要对缓冲区进行复位操作
        return result;
    }

    // 
    void ensureWriteableBytes(size_t len) {
        if(writeableBytes() < len) {
            // 扩容
            makeSpace(len);
        }
    }

    // 把 [data, data + len] 内存上的数据，添加到 writeable 缓冲区中
    void append(const char *data, size_t len) {
        ensureWriteableBytes(len);
        std::copy(data, data + len, beginWirte());
        writerIndex_ += len;
    }

    void append(const void *data, size_t len) {
        append(static_cast<const char *>(data), len);
    }

    void appendInt32(int32_t x) {
        int32_t nw32 = ::htonl(x);
        append(&nw32, sizeof(nw32));
    }

    void prepend(const void *data, size_t len) {
        if(len <= prependableBytes()) {
            readerIndex_ -= len;
            const char *d = (const char *)data;
            std::copy(d, d + len, begin() + readerIndex_);
        }
    }

    char *beginWirte() {
        return begin() + writerIndex_;
    }

    const char *beginWirte() const {
        return begin() + writerIndex_;
    }

    // 从 fd 上读取数据
    ssize_t readFd(int fd, int *saveErrno);
    // 通过 fd 发生数据
    ssize_t writeFd(int fd, int *saveErrno);

private:
    char *begin() {
        // vector 底层数组元素的地址，也就是数组的首地址
        return &*buffer_.begin();
    }

    const char *begin() const {
        // vector 底层数组元素的地址，也就是数组的首地址
        return &*buffer_.begin();
    }

    /**
     * kCheapPrepend 是空闲字节（默认是 8），reader 是读指针，writer 是写指针
     * kCheapPrepend | reader | writer |
     * kCheapPrepend |         len         |
    */
    void makeSpace(size_t len) {
        // 可写大小 + 空闲大小 < 要写大小 + 空闲大小(默认 8 字节)
        if(writeableBytes() + prependableBytes() < len + kCheapPrepend) {
            buffer_.resize(writerIndex_ + len);
        } else {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector <char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};

}   // namespace mymuduo

#endif
