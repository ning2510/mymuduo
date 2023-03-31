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
               const InetAddress &serverAddr)
    : loop_(loop),
      client_(loop_, serverAddr, "ChatClient"),
      dispatcher_(std::bind(&ChatClient::onUnknownMessage, this, _1, _2, _3)),
      codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3))
    {
        dispatcher_.registerMessageCallback<muduo::Answer>(
            std::bind(&ChatClient::onAnswer, this, _1, _2, _3));

        client_.setConnectionCallback(std::bind(&ChatClient::onConnection, this, _1));
        client_.setMessageCallback(
            std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    }

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
    void onConnection(const TcpConnectionPtr &conn) {
        LOG_INFO("%s -> %s is %s",
                  conn->localAddress().toIpPort().c_str(),
                  conn->peerAddress().toIpPort().c_str(),
                  conn->connected() ? "UP" : "DOWN");

        if(conn->connected()) {
            unique_lock<mutex> lock(mutex_);
            conn_ = conn;
        } else {
            unique_lock<mutex> lock(mutex_);
            conn_.reset();
        }
    }
    
    void onUnknownMessage(const TcpConnectionPtr &,
                          const MessagePtr &message,
                          Timestamp)
    {
        LOG_INFO("onUnknownMessage %s", message->GetTypeName().c_str());
    }

    void onAnswer(const TcpConnectionPtr &,
                  const AnswerPtr &message,
                  Timestamp)
    {
        LOG_INFO("onAnswer: %s", message->GetTypeName().c_str());
        cout << "msgid = " << message->msgid() << endl;
        cout << "error = " << message->error() << endl;
        cout << "id = " << message->id() << endl;
    }

    EventLoop *loop_;
    TcpClient client_;
    TcpConnectionPtr conn_;
    ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
    mutex mutex_;
};

int main(int argc, char **argv) {
    string ip = "127.0.0.1";
    uint16_t port = 9999;

    if(argc > 2) {
        port = atoi(argv[1]);
        ip = argv[2];
    }

    EventLoopThread loopThread;
    InetAddress serverAddr(port, ip);

    ChatClient client(loopThread.startLoop(), serverAddr);
    client.connect();

    sleep(2);

    // test Chat
    // muduo::Chat chat;
    // chat.set_msgid(1);
    // chat.set_id(10);
    // chat.set_name("ccc");
    // chat.set_toid(20);
    // chat.set_message("ni hao!");
    // chat.set_time(Timestamp::now().toString());

    // client.send(&chat);

    // sleep(1);

    // // test Add
    // muduo::Add add;
    // add.set_msgid(1);
    // add.set_id(10);
    // add.set_toid(20);

    // client.send(&add);

    // sleep(1);

    // // test CreateGroup
    // muduo::CreateGroup creategroup;
    // creategroup.set_msgid(1);
    // creategroup.set_id(10);
    // creategroup.set_name("ccc");
    // creategroup.set_desc("test cg");

    // client.send(&creategroup);

    // sleep(1);

    // // test Register
    // muduo::Register reg;
    // reg.set_msgid(1);
    // reg.set_username("ccc");
    // reg.set_password("123");

    // client.send(&reg);

    // sleep(1);

    // // test Login
    // muduo::Login login;
    // login.set_msgid(1);
    // login.set_id(10);
    // login.set_password("123");

    // client.send(&login);

    // sleep(1);

    // // test Logout
    // muduo::Logout logout;
    // logout.set_msgid(1);
    // logout.set_id(10);

    // client.send(&logout);

    // sleep(1);

    // test Answer
    muduo::Answer answer;
    answer.set_msgid(1);
    answer.set_error(0);
    answer.set_id(10);

    client.send(&answer);

    int num;
    while(1) {
        cin >> num;
        if(num == 0) break;
    }
    // string line;
    // while(getline(cin, line)) {

    // }

    client.disconnect();
    return 0;
}