#ifndef _ACCEPTOR_H
#define _ACCEPTOR_H

#include <functional>
#include "Socket.h"
#include "Channel.h"

#include "noncopyable.h"

class EventLoop;
class InetAddress;

class Acceptor : noncopyable {
public:
    
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb) {
        newConnectionCallback_ = cb;
    }

    bool listening() const { return listening_; }
    void listen();

private:
    void handleRead();

    // Acceptor 用的就是用户定义的那个 baseLoop，也就是 mainLoop
    EventLoop *loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_;
};

#endif
