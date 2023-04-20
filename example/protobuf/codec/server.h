#ifndef _SERVER_H
#define _SERVER_H

#include "codec.h"
#include "dispatcher.h"
#include "chatMsg.pb.h"

#include "mymuduo/Logger.h"
#include "mymuduo/EventLoop.h"
#include "mymuduo/TcpServer.h"

#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <set>

using namespace std;
using namespace mymuduo;
using namespace std::placeholders;

using ChatPtr = std::shared_ptr<muduo::Chat>;
using AddPtr = std::shared_ptr<muduo::Add>;
using CreateGroupPtr = std::shared_ptr<muduo::CreateGroup>;
using RegisterPtr = std::shared_ptr<muduo::Register>;
using LoginPtr = std::shared_ptr<muduo::Login>;
using LogoutPtr = std::shared_ptr<muduo::Logout>;
using AnswerPtr = std::shared_ptr<muduo::Answer>;

class ChatServer : noncopyable {
public:
    ChatServer(EventLoop *loop, const InetAddress &listenAddr);

    void start() {
        server_.start();
    }

private:

    void onConnection(const TcpConnectionPtr &conn);

    void onUnknownMessage(const TcpConnectionPtr &conn,
                          const MessagePtr &message,
                          Timestamp);

    void onChat(const TcpConnectionPtr &conn,
                const ChatPtr &message,
                Timestamp);

    void onAdd(const TcpConnectionPtr &conn,
               const AddPtr &message,
               Timestamp);

    void onCreateGroup(const TcpConnectionPtr &conn,
                const CreateGroupPtr &message,
                Timestamp);

    void onRegister(const TcpConnectionPtr &conn,
                const RegisterPtr &message,
                Timestamp);

    void onLogin(const TcpConnectionPtr &conn,
                const LoginPtr &message,
                Timestamp);

    void onLogout(const TcpConnectionPtr &conn,
                const LogoutPtr &message,
                Timestamp);

    void onAnswer(const TcpConnectionPtr &conn,
                const AnswerPtr &message,
                Timestamp);

    using ConnectionList = std::set<TcpConnectionPtr>;

    TcpServer server_;
    ConnectionList connections_;
    ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
};

#endif
