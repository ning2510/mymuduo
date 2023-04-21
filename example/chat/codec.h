#ifndef _CODEC_H
#define _CODEC_H

#include "mymuduo/TcpConnection.h"
#include "mymuduo/Logger.h"
#include "mymuduo/Buffer.h"

#include <functional>

using namespace mymuduo;

class Codec : noncopyable {
public:
    using StringMsgCallback = std::function<void(const TcpConnectionPtr &, 
                                                 const std::string &msg,
                                                 Timestamp)>;

    explicit Codec(const StringMsgCallback &cb) : messageCallback_(cb) {}
    
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time) {
        while(buf->readableBytes() >= kHeaderLen) {
            const void *data = buf->peek();
            int32_t nw32 = *static_cast<const int32_t *>(data);
            int32_t len = ntohl(nw32);
            // int32_t len = __bswap_32 (nw32);

            LOG_INFO << "[recv msg] len = " << len << ", nw32 = " << nw32;

            if(len > 65536 || len < 0) {
                LOG_ERROR << "Invalid length = " << len;
                conn->shutdown();
            } else if(buf->readableBytes() >= len + kHeaderLen) {
                buf->retrieve(kHeaderLen);
                std::string message(buf->peek(), len);
                messageCallback_(conn, message, time);
                buf->retrieve(len);
            } else {
                break;
            }
        }
    }

    void send(TcpConnection *conn, const std::string &data) {
        Buffer buf;
        buf.append(data.c_str(), data.size());
        int32_t len = data.size();
        // int32_t nw32 = __bswap_32 (len);
        int32_t nw32 = htonl(len);
        buf.prepend(&nw32, sizeof(nw32));
        std::string message = buf.retrieveAllAsString();

        LOG_INFO << "[send msg] len = " << message.size() << ", nw32 = " << nw32;

        conn->send(message);
    }

private:
    StringMsgCallback messageCallback_;
    static const int kHeaderLen = sizeof(int32_t);
};

#endif
