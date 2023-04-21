#include "server.h"


ChatServer::ChatServer(EventLoop *loop, const InetAddress &listenAddr)
    : server_(loop, listenAddr, "ChatServer"),
      dispatcher_(std::bind(&ChatServer::onUnknownMessage, this, _1, _2, _3)),
      codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3))
{
    dispatcher_.registerMessageCallback<muduo::Chat>(
        std::bind(&ChatServer::onChat, this, _1, _2, _3));

    dispatcher_.registerMessageCallback<muduo::Add>(
        std::bind(&ChatServer::onAdd, this, _1, _2, _3));

    dispatcher_.registerMessageCallback<muduo::CreateGroup>(
        std::bind(&ChatServer::onCreateGroup, this, _1, _2, _3));

    dispatcher_.registerMessageCallback<muduo::Register>(
        std::bind(&ChatServer::onRegister, this, _1, _2, _3));

    dispatcher_.registerMessageCallback<muduo::Login>(
        std::bind(&ChatServer::onLogin, this, _1, _2, _3));

    dispatcher_.registerMessageCallback<muduo::Logout>(
        std::bind(&ChatServer::onLogout, this, _1, _2, _3));

    dispatcher_.registerMessageCallback<muduo::Answer>(
        std::bind(&ChatServer::onAnswer, this, _1, _2, _3));

    server_.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));
    server_.setMessageCallback(std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));

    server_.setThreadNum(2);
}


void ChatServer::onConnection(const TcpConnectionPtr &conn) {
    LOG_INFO << conn->localAddress().toIpPort() << " -> " << conn->peerAddress().toIpPort() 
             << " is " << conn->connected() ? "UP" : "DOWN";

    if(conn->connected()) {
        connections_.insert(conn);
    } else {
        connections_.erase(conn);
    }
}

void ChatServer::onUnknownMessage(const TcpConnectionPtr &conn,
                        const MessagePtr &message,
                        Timestamp)
{
    LOG_INFO << "onUnknownMessage " << message->GetTypeName();
    conn->shutdown();
}

void ChatServer::onChat(const TcpConnectionPtr &conn,
            const ChatPtr &message,
            Timestamp)
{
    LOG_INFO << "onChat: " << message->GetTypeName();
    cout << "msgid = " << message->msgid() << endl;
    cout << "id = " << message->id() << endl;
    cout << "name = " << message->name() << endl;
    cout << "toid = " << message->toid() << endl;
    cout << "message = " << message->message() << endl;
    cout << "time = " << message->time() << endl;
}

void ChatServer::onAdd(const TcpConnectionPtr &conn,
            const AddPtr &message,
            Timestamp)
{
    LOG_INFO << "onAdd: " << message->GetTypeName();
    cout << "msgid = " << message->msgid() << endl;
    cout << "id = " << message->id() << endl;
    cout << "toid = " << message->toid() << endl;
}

void ChatServer::onCreateGroup(const TcpConnectionPtr &conn,
            const CreateGroupPtr &message,
            Timestamp)
{
    LOG_INFO << "onCreateGroup: " << message->GetTypeName();
    cout << "msgid = " << message->msgid() << endl;
    cout << "id = " << message->id() << endl;
    cout << "name = " << message->name() << endl;
    cout << "desc = " << message->desc() << endl;
}

void ChatServer::onRegister(const TcpConnectionPtr &conn,
            const RegisterPtr &message,
            Timestamp)
{
    LOG_INFO << "onRegister: " << message->GetTypeName();
    cout << "msgid = " << message->msgid() << endl;
    cout << "username = " << message->username() << endl;
    cout << "password = " << message->password() << endl;
}

void ChatServer::onLogin(const TcpConnectionPtr &conn,
            const LoginPtr &message,
            Timestamp)
{
    LOG_INFO << "onLogin: " << message->GetTypeName();
    cout << "msgid = " << message->msgid() << endl;
    cout << "id = " << message->id() << endl;
    cout << "password = " << message->password() << endl;
}

void ChatServer::onLogout(const TcpConnectionPtr &conn,
            const LogoutPtr &message,
            Timestamp)
{
    LOG_INFO << "onLogout: " << message->GetTypeName();
    cout << "msgid = " << message->msgid() << endl;
    cout << "id = " << message->id() << endl;
}

void ChatServer::onAnswer(const TcpConnectionPtr &conn,
            const AnswerPtr &message,
            Timestamp)
{
    LOG_INFO << "onAnswer: " << message->GetTypeName();
    cout << "msgid = " << message->msgid() << endl;
    cout << "error = " << message->error() << endl;
    cout << "id = " << message->id() << endl;

    muduo::Answer answer;
    answer.set_msgid(1);
    answer.set_error(0);
    answer.set_id(10);
    
    for(auto it = connections_.begin(); it != connections_.end(); it++) {
        codec_.send(*it, answer);
    }

    
}

int main(int argc, char **argv) {
    mymuduo::initLog("server_log");
    
    uint16_t port = 9999;

    if(argc > 1) {
        port = atoi(argv[1]);
    }

    cout << "[conf] use port = " << port << endl;

    EventLoop loop;
    InetAddress serverAddr(port);
    ChatServer server(&loop, serverAddr);
    server.start();
    loop.loop();

    return 0;
}