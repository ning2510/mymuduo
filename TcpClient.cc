#include "TcpClient.h"
#include "Logger.h"
#include "EventLoop.h"

#include <strings.h>

using namespace std::placeholders;

namespace detail {
    void removeConnection(EventLoop *loop, const TcpConnectionPtr &conn) {

    }
}

void defaultConnectionCallback(const TcpConnectionPtr &conn) {
    LOG_INFO("%s -> %s is %s", 
             conn->localAddress().toIpPort().c_str(),
             conn->peerAddress().toIpPort().c_str(),
             (conn->connected() ? "UP" : "DOWN"));
}

void defaultMessageCallback(const TcpConnectionPtr&, Buffer* buf, Timestamp) {
    buf->retrieveAll();
}

TcpClient::TcpClient(EventLoop *loop, 
              const InetAddress &serverAddr, 
              const std::string &nameArg) 
              : loop_(loop),
                connector_(new Connector(loop, serverAddr)),
                name_(nameArg),
                connectionCallback_(defaultConnectionCallback),
                messageCallback_(defaultMessageCallback),
                retry_(false),
                connect_(false),
                nextConnId_(1)
{
    connector_->setNewConnectionCallback(std::bind(&TcpClient::newConnection, this, _1));
}

TcpClient::~TcpClient() {
    LOG_INFO("TcpClient::~TcpClient [%s]", name_.c_str());

    TcpConnectionPtr conn;
    bool unique = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        unique = connection_.unique();
        conn = connection_;
    }

    if(conn) {
        CloseCallback cb = std::bind(&detail::removeConnection, loop_, _1);
        loop_->runInLoop(
                std::bind(&TcpConnection::setCloseCallback, conn, cb));
        if(unique) {
            conn->forceClose();
        }
    
    } else {
        connector_->stop();
    }

}

void TcpClient::connect() {
    LOG_INFO("TcpClient::connect [%s] - connecting to %s", 
            name_.c_str(),
            connector_->serverAddress().toIpPort().c_str());

    connect_ = true;
    connector_->start();
}

void TcpClient::disconnect() {
    connect_ = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if(connection_) {
            connection_->shutdown();
        }
    }
}

void TcpClient::stop() {
    connect_ = false;
    connector_->stop();
}

void TcpClient::newConnection(int sockfd) {
    loop_->assertInLoopThread();

    struct sockaddr_in peeraddr;
    ::bzero(&peeraddr, sizeof(peeraddr));
    socklen_t addrlen = sizeof(peeraddr);
    if(::getpeername(sockfd, (sockaddr *)&peeraddr, &addrlen) > 0) {
        LOG_ERROR("getpeername error = %d", errno);
    }

    InetAddress peerAddr(peeraddr);
    char buf[32];
    snprintf(buf, sizeof(buf), ":%s#%d", peerAddr.toIpPort().c_str(), nextConnId_);
    nextConnId_++;
    std::string connName = name_ + buf;


    struct sockaddr_in localaddr;
    ::bzero(&localaddr, sizeof(localaddr));
    addrlen = sizeof(localaddr);
    if(::getsockname(sockfd, (sockaddr *)&localaddr, &addrlen) > 0) {
        LOG_ERROR("getsockname error = %d", errno);
    }

    InetAddress localAddr(localaddr);

    TcpConnectionPtr conn(new TcpConnection(loop_, connName, sockfd, localAddr, peerAddr));
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(std::bind(&TcpClient::removeConnection, this, _1));

    {
        std::unique_lock<std::mutex> lock(mutex_);
        connection_ = conn;
    }

    conn->connectEstablished();
}

void TcpClient::removeConnection(const TcpConnectionPtr &conn) {
    loop_->assertInLoopThread();

    {
        std::unique_lock<std::mutex> lock(mutex_);
        connection_.reset();
    }

    loop_->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    if(retry_ && connect_) {
        LOG_INFO("TcpClient::removeConnection [%s] reconnecting to %s",
                 name_.c_str(), connector_->serverAddress().toIpPort().c_str());
        connector_->restart();
    }
}