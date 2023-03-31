#include "client.h"


ChatClient::ChatClient(EventLoop *loop,
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

void ChatClient::onConnection(const TcpConnectionPtr &conn) {
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

void ChatClient::onUnknownMessage(const TcpConnectionPtr &,
                            const MessagePtr &message,
                            Timestamp)
{
    LOG_INFO("onUnknownMessage %s", message->GetTypeName().c_str());
}

void ChatClient::onAnswer(const TcpConnectionPtr &,
                const AnswerPtr &message,
                Timestamp)
{
    LOG_INFO("onAnswer: %s", message->GetTypeName().c_str());
    cout << "msgid = " << message->msgid() << endl;
    cout << "error = " << message->error() << endl;
    cout << "id = " << message->id() << endl;
}


int main(int argc, char **argv) {
    string ip = "127.0.0.1";
    uint16_t port = 9999;

    if(argc > 2) {
        port = atoi(argv[1]);
        ip = argv[2];
    }

    LOG_INFO("[conf] use ip = %s, port = %d", ip.c_str(), port);

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