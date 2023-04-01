#ifndef _TCPCLIENT_H
#define _TCPCLIENT_H

#include "TcpConnection.h"

#include <string>
#include <mutex>

class Connector;

using ConnectorPtr = std::shared_ptr<Connector>;

class TcpClient : noncopyable {
public:
    TcpClient(EventLoop *loop, 
              const InetAddress &serverAddr, 
              const std::string &nameArg);

    ~TcpClient();

    void connect();
    void disconnect();
    void stop();

    bool isConnected() const { return connector_->isConnected(); }

    TcpConnectionPtr connection() {
        std::unique_lock<std::mutex> lock(mutex_);
        return connection_;
    }

    EventLoop *getLoop() const { return loop_; }
    bool retry() const { return retry_; }
    void enableRetry() { retry_ = true; }

    const std::string &name() const { return name_; }

    void setConnectionCallback(const ConnectionCallback &cb) {
        connectionCallback_ = std::move(cb);
    }
    void setMessageCallback(const MessageCallback &cb) {
        messageCallback_ = std::move(cb);
    }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
        writeCompleteCallback_ = std::move(cb);
    }

private:
    void newConnection(int sockfd);
    void removeConnection(const TcpConnectionPtr &conn);

    EventLoop *loop_;
    ConnectorPtr connector_;
    const std::string name_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;

    bool retry_;
    bool connect_;

    int nextConnId_;
    std::mutex mutex_;

    TcpConnectionPtr connection_;
};

#endif
