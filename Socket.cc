#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <string.h>

namespace mymuduo {

Socket::~Socket() {
    ::close(sockfd_);
}

void Socket::bindAddress(const InetAddress &localaddr) {
    if(::bind(sockfd_, (sockaddr *)localaddr.getSockAddr(), sizeof(sockaddr_in)) != 0) {
        LOG_FATAL << "bins sockfd: " << sockfd_ << " fail";
    }
}

void Socket::listen() {
    if(::listen(sockfd_, 1024) != 0) {
        LOG_FATAL << "listen sockfd: " << sockfd_ << " fail";
    }
}

int Socket::accept(InetAddress *peeraddr) {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    bzero(&addr, sizeof(addr));
    
    // accept4 中的 最后一个参数：给 connfd 设置为非阻塞，当该进程 fork 后，该进程的 connfd 会被删除
    int connfd = ::accept4(sockfd_, (sockaddr *)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if(connfd >= 0) {
        peeraddr->setSockAddr(addr);
    } else {
        LOG_ERROR << "connfd error";
    }

    return connfd;
}

void Socket::shutdownWrite() {
    if(::shutdown(sockfd_, SHUT_WR) < 0) {
        LOG_ERROR << "Socket::shutdownwrite error";
    }
}

void Socket::setTcpNoDelay(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setReusePort(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}

}   // namespace mymuduo