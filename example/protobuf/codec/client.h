#ifndef _CLIENT_H
#define _CLIENT_H

#include "dispatcher.h"
#include "codec.h"
#include "chatMsg.pb.h"

#include "mymuduo/Logger.h"
#include "mymuduo/EventLoop.h"
#include "mymuduo/TcpClient.h"
#include "mymuduo/EventLoopThread.h"

#include <stdio.h>
#include <unistd.h>
#include <mutex>

using namespace std;
using namespace std::placeholders;

using ChatPtr = std::shared_ptr<muduo::Chat>;
using AddPtr = std::shared_ptr<muduo::Add>;
using CreateGroupPtr = std::shared_ptr<muduo::CreateGroup>;
using RegisterPtr = std::shared_ptr<muduo::Register>;
using LoginPtr = std::shared_ptr<muduo::Login>;
using LogoutPtr = std::shared_ptr<muduo::Logout>;
using AnswerPtr = std::shared_ptr<muduo::Answer>;

class ChatClient : noncopyable {
public:
    ChatClient(EventLoop *loop,
               const InetAddress &serverAddr);

    void connect() {
        client_.connect();
    }

    void disconnect() {
        client_.disconnect();
    }

    void send(google::protobuf::Message *messageToSend) {
        if(conn_) {
            codec_.send(conn_, *messageToSend);
        }
    }

private:
    void onConnection(const TcpConnectionPtr &conn);
    
    void onUnknownMessage(const TcpConnectionPtr &,
                          const MessagePtr &message,
                          Timestamp);

    void onAnswer(const TcpConnectionPtr &,
                  const AnswerPtr &message,
                  Timestamp);

    EventLoop *loop_;
    TcpClient client_;
    TcpConnectionPtr conn_;
    ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
    mutex mutex_;
};

#endif
