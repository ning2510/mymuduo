#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

namespace mymuduo {

static int createNonblocking() {
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(sockfd < 0) {
        LOG_FATAL << "listen socket create err : " << errno;
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop),
      acceptSocket_(createNonblocking()),
      acceptChannel_(loop, acceptSocket_.fd()),
      listening_(false) {

    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr);
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen() {
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

// listenfd 有事件发生时（也就是有新用户连接了）就会调用 handleRead
void Acceptor::handleRead() {
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd > 0) {
        if(newConnectionCallback_) {
            // 轮询找到 subLoop，唤醒，分发当前新客户端的 Channel
            newConnectionCallback_(connfd, peerAddr);
        } else {
            ::close(connfd);
        }
    } else {
        LOG_ERROR << "accept err : " << errno;
        if(errno == EMFILE) {
            LOG_ERROR << "sockfd reached limit";
        }
    }
}

}   // namespace mymuduo