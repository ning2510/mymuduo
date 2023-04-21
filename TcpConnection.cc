#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <functional>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <strings.h>
#include <string>

namespace mymuduo {

static EventLoop *CheckLoopNotNull(EventLoop *loop) {
    if(loop == nullptr) {
        LOG_FATAL << "TcpConnection is null !";
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                    const std::string &nameArg,
                    int sockfd,
                    const InetAddress &localAddr,
                    const InetAddress &peerAddr) 
        : loop_(CheckLoopNotNull(loop)),
          name_(nameArg),
          state_(kConnecting),
          reading_(true),
          socket_(new Socket(sockfd)),
          channel_(new Channel(loop, sockfd)),
          localAddr_(localAddr),
          peerAddr_(peerAddr),
          highWaterMark_(64 * 1024 * 1024)  // 64M
{
    // 下面给 Channel 设置相应的回调函数，当 poller 监听到 channel 感兴趣的事件，就会调用 channel 对应的回调
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO << "TcpConnection::ctor[" << name_ << "] at fd = " << sockfd;
    socket_->setKeepAlive(true);
}


TcpConnection::~TcpConnection() {
    LOG_INFO << "TcpConnection::dtor[" << name_ << "] at fd = " << channel_->fd() << ", state = " << state_;
}

void TcpConnection::send(const std::string &buf) {
    if(state_ == kConnected) {
        if(loop_->isInLoopThread()) {
            sendInLoop(buf.c_str(), buf.size());
        } else {
            sendMsg.clear();
            sendMsg = buf;
            sendMsgLen = sendMsg.size();

            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, sendMsg.c_str(), sendMsgLen));
        }
    }
}

/**
 * 发送数据：应用写的快，而内核发送数据慢，需要把待发送的数据写入缓冲区，然后设置水位回调
*/
void TcpConnection::sendInLoop(const void *data, size_t len) {
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    LOG_INFO << "send data = " << (const char *)data << ", len = " << len;
    // 如果之前调用过该 TcpConnection 的 shutdown，就不能再发送了
    if(state_ == kDisconnected) {
        LOG_ERROR << "disconnected, give up writing!";
        return ;
    }

    // channel 第一次开始写数据，而且发送缓冲区没有待发送数据
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = ::write(channel_->fd(), data, len);
        if(nwrote >= 0) {
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_) {
                // 如果一次性就把数据全部发送完了，就不用再给 channel 设置 EPOLLOUT 事件了
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        } else {    // nworte < 0
            nwrote = 0;
            if(errno != EWOULDBLOCK) {
                LOG_ERROR << "TcpConnection::sendInLoop error";

                if(errno == EPIPE || errno == ECONNRESET) {     // SIGPIPE  RESET
                    faultError = true;
                }
            }
        }
    }

    // 说明当前这次 write 并没有把全部数据发送出去，那么剩余的数据就要保存到缓冲区当中，然后给 Channel 注册 EPOLLOUT 事件
    // 之后 Poller 发送 TCP 的发送缓冲区中有内容需要发送，就会通知相应的 sockfd，然后调用 channel 的 handleWrite 方法
    // 也就是调用 TcpConnection::handleWrite 方法，把发送缓冲区中的数据全部发送
    if(!faultError && remaining > 0) {
        // 目前发送缓冲区剩余的待发送数据的长度
        size_t oldLen = outputBuffer_.readableBytes();
        if(oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_) {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append((char *)data + nwrote, remaining);
        if(!channel_->isWriting()) {
            // 注册 channel 的写事件
            channel_->enableWriting();
        } 
    }
}

// 关闭连接
void TcpConnection::shutdown() {
    if(state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop() {
    if(!channel_->isWriting()) {    // 说明当前 outputBuffer 中的数据已经全部发送完成
        socket_->shutdownWrite();   // 关闭写端，会触发 EPOLLHUP 事件
    }
}


// 连接建立
void TcpConnection::connectEstablished() {
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();  // 向 Poller 注册 channel 的 EPOLLIN 事件

    // 新连接建立，执行回调（这个回调是用户自定义的）
    connectionCallback_(shared_from_this());
}

// 连接销毁
void TcpConnection::connectDestroyed() {
    if(state_ == kConnected) {
        setState(kDisconnected);
        channel_->disableAll();     // 把 channel 所有感兴趣的事件从 Poller 中删除
        connectionCallback_(shared_from_this());
    }

    // 把 channel 从 Poller 中删除掉
    channel_->remove();
}

void TcpConnection::handleRead(Timestamp receiveTime) {
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);

    if(n > 0) {
        // 以建立连接的用户，有可读事件发生，调用用户传入的回调操作 onMessage()
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    } else if(n == 0) {
        handleClose();
    } else {
        errno = savedErrno;
        LOG_ERROR << "TcpConnection::handleRead error";
        handleError();
    }
}

void TcpConnection::handleWrite() {
    if(channel_->isWriting()) {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if(n > 0) {
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes() == 0) {
                channel_->disableWriting();
                if(writeCompleteCallback_) {
                    // 唤醒 loop_ 对应的 thread 线程执行回调
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if(state_ == kDisconnecting) {
                    shutdownInLoop();
                }
            }
        } else {
            LOG_ERROR << "TcpConnection::handleWrite error";
        }
    } else {
        LOG_ERROR << "TcpConnection fd = " << channel_->fd() << " is down, now more writing";
    }
}

// Poller 通知 Channel 调用 closeCallback_ 回调，该回调就是 handleClose 方法
// 在 handleClose 方法中分别调用了 用户注册的回调(connectionCallback_) 和 TcpServer 注册的关闭回调(closeCallback_)
// 执行 TcpServer 注册的回调也就是 TcpServer::removeConnection 方法
void TcpConnection::handleClose() {
    LOG_INFO << "TcpConnection::handleClose fd = " << channel_->fd() << ", state = " << state_;
    setState(kDisconnected);
    channel_->disableAll();     // 删除所有感兴趣的事件

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);   // 执行用户注册的连接关闭的回调
    closeCallback_(connPtr);        // 关闭连接的回调（该回调是 TcpServer 注册的）
}

void TcpConnection::handleError() {
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        err = errno;
    } else {
        err = optval;
    }
    LOG_ERROR << "TcpConnection::handleError name: " << name_ << " - SO_ERROR: " << err;
}

void TcpConnection::forceClose() {
    if (state_ == kConnected || state_ == kDisconnecting) {
        setState(kDisconnecting);
        loop_->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
    }
}

void TcpConnection::forceCloseInLoop() {
    loop_->assertInLoopThread();
    if (state_ == kConnected || state_ == kDisconnecting) {
        // as if we received 0 byte in handleRead();
        handleClose();
    }
}

}   // namespace mymuduo