#ifndef _TCPCONNECTION_H
#define _TCPCONNECTION_H

#include <memory>
#include <string>
#include <atomic>

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

class Channel;
class EventLoop;
class Socket;

/**
 * 当有一个新用户连接时，机会通过 accept 拿到 connfd，之后把 connfd 包装成 TcpConnection 对象，
 * 然后给 TcpConnection 设置回调，然后再把回调设置给相应的 Channel，然后 Channel 被注册到 Poller 上，
 * Poller 监听到相应事件后会调用 Channel 的回调，进而调用 TcpConnection 的回调
*/
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(EventLoop *loop,
                    const std::string &nameArg,
                    int sockfd,
                    const InetAddress &localAddr,
                    const InetAddress &peerAddr);
    ~TcpConnection();

    EventLoop *getLoop() const { return loop_; }
    const std::string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress &peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    // 发送数据
    void send(const std::string &buf);
    // 关闭连接
    void shutdown();

    void forceClose();

    void setConnectionCallback(const ConnectionCallback &cb) {
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback &cb) {
        messageCallback_ = cb;
    }

    void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
        writeCompleteCallback_ = cb;
    }

    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark) {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

    void setCloseCallback(const CloseCallback &cb) {
        closeCallback_ = cb;
    }

    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();

private:
    enum StateE {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };
    
    void setState(StateE state) { state_ = state; }

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void *data, size_t len);

    void shutdownInLoop();

    void forceCloseInLoop();

    EventLoop *loop_;   // 这里是某个 subLoop
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    // 这里和 Acceptor 类似，Accept 在 mainLoop 中，TcpConnection 在 subLoop 中
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;             // 有新连接时的回调
    MessageCallback messageCallback_;                   // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;       // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;

    Buffer inputBuffer_;        // 接收数据的缓冲区
    Buffer outputBuffer_;       // 发送数据的缓冲区

    std::string sendMsg;
    size_t sendMsgLen;
};


#endif
