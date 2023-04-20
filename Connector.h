#ifndef _CONNECTOR_H
#define _CONNECTOR_H

#include "noncopyable.h"
#include "InetAddress.h"

#include <functional>
#include <memory>

namespace mymuduo {

class Channel;
class EventLoop;

class Connector : noncopyable, public std::enable_shared_from_this<Connector> {
public:
    typedef std::function<void(int sockfd)> NewConnectionCallback;

    Connector(EventLoop *loop, const InetAddress &serverAddr);
    ~Connector();

    void setNewConnectionCallback(const NewConnectionCallback &cb) {
        newConnectionCallback_ = std::move(cb);
    }

    void start();
    void restart();
    void stop();

    bool isConnected() const {
        return state_ == kConnected;
    }

    const InetAddress &serverAddress() const { return serverAddr_; }

private:
    enum States {
        kDisconnected,
        kConnecting,
        kConnected
    };
    
    static const int kMaxRetryDelayMs = 30 * 1000;
    static const int kInitRetryDelayMs = 500;

    void setState(States state) { state_ = state; }

    void startInLoop();
    void stopInLoop();

    void connect();
    void connecting(int sockfd);

    void handleWrite();
    void handleError();

    void retry(int sockfd);

    void resetChannel();
    int removeAndResetChannel();

    EventLoop *loop_;
    InetAddress serverAddr_;
    bool connect_;
    States state_;
    
    std::unique_ptr<Channel> channel_;
    NewConnectionCallback newConnectionCallback_;

    int retryDelayMs_;
};

}   // namespace mymuduo

#endif
