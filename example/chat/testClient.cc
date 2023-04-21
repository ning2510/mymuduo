#include "mymuduo/TcpClient.h"
#include "mymuduo/Logger.h"
#include "mymuduo/EventLoop.h"
#include "mymuduo/EventLoopThread.h"
#include "codec.h"

// #include "TcpClient.h"
// #include "Logger.h"
// #include "EventLoop.h"
// #include "EventLoopThread.h"

#include <iostream>
#include <mutex>

using namespace std;
using namespace mymuduo;
using namespace std::placeholders;

class EchoClient {
public:
    EchoClient(EventLoop *loop, const InetAddress &serverAddr)
                : client_(loop, serverAddr, "ChatClient"),
                  codec_(std::bind(&EchoClient::onMessage, this, _1, _2, _3))
    {
        client_.setConnectionCallback(
            std::bind(&EchoClient::onConnection, this, _1));
        client_.setMessageCallback(
            std::bind(&Codec::onMessage, &codec_, _1, _2, _3));
    }

    ~EchoClient() {}

    void connect() {
        client_.connect();
    }

    void disconnect() {
        client_.disconnect();
    }

    void send(const string &msg) {
        {
            unique_lock<mutex> lock(mutex_);
            if(conn_) {
                codec_.send(conn_.get(), msg);
            }
        }
    }

private:
    void onConnection(const TcpConnectionPtr &conn) {
        if(conn->connected()) {
            LOG_INFO << "Connection UP : " << conn->peerAddress().toIpPort();
            {
                unique_lock<mutex> lock(mutex_);
                conn_ = conn;
            }
        } else {
            LOG_INFO << "Connection DOWN : " << conn->peerAddress().toIpPort();
            {
                unique_lock<mutex> lock(mutex_);
                conn_.reset();
            }
        }
    }

    void onMessage(const TcpConnectionPtr &conn, const std::string &msg, Timestamp time) {
        cout << "recv msg = " << msg << endl;
        // conn->shutdown();
    }

    Codec codec_;
    EventLoop *loop_;
    TcpClient client_;
    TcpConnectionPtr conn_;
    mutex mutex_;
};

// ./client ip port
int main(int argc, char **argv) {
    mymuduo::initLog("client_log");

    string ip = "127.0.0.1";
    uint16_t port = 9999;

    if(argc > 2) {
        ip = argv[1];
        port = atoi(argv[2]);
    }

    EventLoopThread loopThread;

    InetAddress serverAddr(port, ip);

    EchoClient client(loopThread.startLoop(), serverAddr);
    client.connect();

    string msg;
    while(1) {
        cout << "Please input: ";
        cin >> msg;
        client.send(msg);
    }
    

    client.disconnect();


    return 0;
}