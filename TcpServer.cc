#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <functional>
#include <strings.h>

namespace mymuduo {

static EventLoop *CheckLoopNotNull(EventLoop *loop) {
    if(loop == nullptr) {
        LOG_FATAL << "mainLoop is null !";
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop, 
                const InetAddress &listenAddr,
                const std::string &nameArg,
                Option option)
                : loop_(CheckLoopNotNull(loop)),
                  ipPort_(listenAddr.toIpPort()),
                  name_(nameArg),
                  acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
                  threadPool_(new EventLoopThreadPool(loop, name_)),
                  connectionCallback_(),
                  messageCallback_(),
                  nextConnId_(1),
                  started_(0) {

    // 当有新用户连接时，会执行 TcpServer::newConnection 回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, 
        std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer() {
    for(auto &item : connections_) {
        // 这个局部的 shared_ptr 智能指针对象 conn，会自动释放 new 出来的 TcpConnection 对象资源
        TcpConnectionPtr conn(item.second);
        item.second.reset();

        // 销毁连接
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

// 设置 subLoop 的个数
void TcpServer::setThreadNum(int numThreads) {
    threadPool_->setThreadNum(numThreads);
}

// 开启服务器监听
void TcpServer::start() {
    if(started_++ == 0) {   // 防止一个 TcpServer 对象对 start 多次
        threadPool_->start(threadInitCallback_);        // 启动底层的 loop 线程池（启动所有的 subLoop ）
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr) {
    // 轮询算法，选择一个 subLoop，来管理 channel
    EventLoop *ioLoop = threadPool_->GetNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO << "TcpServer::newConnection [" << name_ <<  "] - new connection [" 
             << connName << "]" << " from " <<peerAddr.toIpPort();

    // 通过 sockfd 获取其绑定的本机的 ip 地址及端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof(local));
    socklen_t addrlen = sizeof(local);
    if(::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0) {
        LOG_ERROR << "sockets::getLocalAddr";
    }

    InetAddress localAddr(local);

    // 根据连接成功的 sockfd，创建 TcpConnection 连接对象
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));

    connections_[connName] = conn;

    // 下面的回调都是用户设置给 TcpServer 的，然后 TcpServer 设置给 TcpConnection，TcpConnection 又设置给 Channel
    // 然后 Channel 注册到 Poller 中，当 Poller 监听到对应的事件就会通知 Channel 调用回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    
    // 设置如何关闭连接的回调
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    // 直接调用 TcpConnection::connectEstablished
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn) {
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn) {
    LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_ << "] - connection " << conn->name();

    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));

}

}   // namespace mymuduo