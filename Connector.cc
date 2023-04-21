#include "Connector.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <errno.h>

namespace mymuduo {

static int createNonblocking() {
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(sockfd < 0) {
        LOG_FATAL << "listen socket create err : " << errno;
    }
    return sockfd;
}

Connector::Connector(EventLoop *loop, const InetAddress &serverAddr)
    : loop_(loop),
      serverAddr_(serverAddr),
      connect_(false),
      state_(kDisconnected),
      retryDelayMs_(kInitRetryDelayMs)
{
    LOG_INFO << "Connector [" << this << "] constructor";
}

Connector::~Connector() {
    LOG_INFO << "Connector [" << this << "] destructor";
}

void Connector::start() {
    connect_ = true;
    loop_->runInLoop(std::bind(&Connector::startInLoop, this));
}

void Connector::startInLoop() {
    loop_->assertInLoopThread();
    if(state_ == kDisconnected) {
        if(connect_) {
            connect();
        } else {
            LOG_DEBUG << "Connector::startInLoop don't connect";
        }
    }
}

void Connector::restart() {
    loop_->assertInLoopThread();
    setState(kDisconnected);
    retryDelayMs_ = kInitRetryDelayMs;
    connect_ = true;
    startInLoop();
}

void Connector::stop() {
    connect_ = false;
    loop_->queueInLoop(std::bind(&Connector::stopInLoop, this));
}

void Connector::stopInLoop() {
    loop_->assertInLoopThread();
    if(state_ == kConnecting) {
        setState(kConnected);
        int sockfd = removeAndResetChannel();
        retry(sockfd);
    }
}

void Connector::connect() {
    int sockfd = createNonblocking();
    socklen_t len = sizeof(sockaddr_in);
    int ret = ::connect(sockfd, (sockaddr *)serverAddr_.getSockAddr(), len);
    int savedErrno = (ret == 0) ? 0 : errno;
    LOG_INFO << "errno = " << savedErrno;
    switch (savedErrno) {
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN:
        connecting(sockfd);
        break;

        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
        retry(sockfd);
        break;

        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
        LOG_ERROR << "connect error in Connector::startInLoop " << savedErrno;
        ::close(sockfd);
        break;

        default:
        LOG_ERROR << "Unexpected error in Connector::startInLoop " << savedErrno;
        ::close(sockfd);
        // connectErrorCallback_();
        break;
    }
}

void Connector::connecting(int sockfd) {
    setState(kConnecting);
    channel_.reset(new Channel(loop_, sockfd));
    channel_->setWriteCallback(std::bind(&Connector::handleWrite, this));
    channel_->setErrorCallback(std::bind(&Connector::handleError, this));

    /**
     *   - 如果当连接可用后，且缓存区不满的情况下，调用 epoll_ctl 将 fd 重新注册到 epoll 事件池
     * (使用EPOLL_CTL_MOD)，这时也会触发 EPOLLOUT 事件
     */
    channel_->enableWriting();
}

void Connector::handleWrite() {
    
    if(state_ == kConnecting) {
        int sockfd = removeAndResetChannel();
        int err;
        int optval;
        socklen_t optlen = sizeof(optval);
        if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
            err =  errno;
        } else {
            err = optval;
        }
        if(err) {
            retry(sockfd);
        } else {
            setState(kConnected);
            if(connect_) {
                newConnectionCallback_(sockfd);
            } else {
                ::close(sockfd);
            }
        }
    }
}

void Connector::handleError() {
    LOG_ERROR << "Connector::handleError state = " << state_;
    if(state_ == kConnecting) {
        int sockfd = removeAndResetChannel();
        int err;
        int optval;
        socklen_t optlen = sizeof(optval);
        if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
            err =  errno;
        } else {
            err = optval;
        }
        retry(sockfd);
    }
}

void Connector::retry(int sockfd) {
    ::close(sockfd);
    setState(kDisconnected);

    /* retry is disabled, because not add TimerQueue */

    // if(connect_) {
    //     LOG_INFO << "Connector::retry connect to " << serverAddr_.toIpPort();
    //     startInLoop();
    // }
}

void Connector::resetChannel() {
    channel_.reset();
}

int Connector::removeAndResetChannel() {
    channel_->disableAll();
    channel_->remove();
    int sockfd = channel_->fd();

    loop_->queueInLoop(std::bind(&Connector::resetChannel, this));
    return sockfd;
}

}   // namespace mymuduo