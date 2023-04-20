#include "mymuduo/TcpServer.h"
#include "mymuduo/Logger.h"

// #include "TcpServer.h"
// #include "Logger.h"

#include <string>
#include <functional>

using namespace mymuduo;
using namespace std::placeholders;

class EchoServer {
public:
    EchoServer(EventLoop *loop, 
                const InetAddress &addr,
                const std::string &name)
            : server_(loop, addr, name),
              loop_(loop)
{
    // 注册回调函数
    server_.setConnectionCallback(std::bind(&EchoServer::onConnection, this, _1));
    server_.setMessageCallback(std::bind(&EchoServer::onMessage, this, _1, _2, _3));

    // 设置合适的 loop 线程数量
    server_.setThreadNum(2);
}

    void start() {
        server_.start();
    }

private:
    // 连接建立或断开的回调
    void onConnection(const TcpConnectionPtr &conn) {
        if(conn->connected()) {
            LOG_INFO("Connection UP : %s", conn->peerAddress().toIpPort().c_str());
        } else {
            LOG_INFO("Connection DOWN : %s", conn->peerAddress().toIpPort().c_str());
        }
    }
    
    // 可读写事件回调
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time) {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        // conn->shutdown();
    }

    EventLoop *loop_;
    TcpServer server_;
};

int main(int argc, char **argv) {
    uint16_t port = 9999;
    if(argc > 1) {
	    port = atoi(argv[1]);	
    }    
    std::cout << "listen port = " << port << std::endl;
    EventLoop loop;
    InetAddress addr(port);

    EchoServer server(&loop, addr, "test111");
    server.start();
    loop.loop();    // 启动 mainLoop 的 Poller

    return 0;
}
