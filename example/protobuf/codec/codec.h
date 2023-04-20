#ifndef _CODEC_H
#define _CODEC_H

#include "mymuduo/Buffer.h"
#include "mymuduo/TcpConnection.h"

#include <google/protobuf/message.h>

using namespace mymuduo;
using MessagePtr = std::shared_ptr<google::protobuf::Message>;

class ProtobufCodec : noncopyable {
public:
    enum ErrorCode {
        kNoError = 0,
        kInvalidLength,
        kInvalidNameLen,
        kUnknownMessageType,
        kParseError,
    };

    using ProtobufMessageCallback = std::function<void(const TcpConnectionPtr &,
                                                       const MessagePtr &,
                                                       Timestamp)>;

    using ErrorCallback = std::function<void(const TcpConnectionPtr &,
                                             Buffer *,
                                             Timestamp,
                                             ErrorCode)>;

    explicit ProtobufCodec(const ProtobufMessageCallback &messageCb)
        : messageCallback_(messageCb),
          errorCallback_(defaultErrorCallback) {}

    ProtobufCodec(const ProtobufMessageCallback &messageCb, const ErrorCallback &errorCb)
        : messageCallback_(messageCb),
          errorCallback_(errorCb) {}

    void onMessage(const TcpConnectionPtr &conn,
                   Buffer *buf,
                   Timestamp receiveTime);

    void send(const TcpConnectionPtr &conn,
              const google::protobuf::Message &message) {
        
        Buffer buf;
        fillEmptyBuffer(&buf, message);
        std::string msg = buf.retrieveAllAsString();
        conn->send(msg);
    }

    static const std::string &errorCodeToString(ErrorCode errorCode);
    static void fillEmptyBuffer(Buffer *buf, const google::protobuf::Message &message);
    static google::protobuf::Message *createMessage(const std::string &typeName);
    static MessagePtr parse(const char *buf, int len, ErrorCode *error);

private:
    static void defaultErrorCallback(const TcpConnectionPtr &,
                                     Buffer *,
                                     Timestamp,
                                     ErrorCode);

    ProtobufMessageCallback messageCallback_;
    ErrorCallback errorCallback_;

    const static int kHeaderLen = sizeof(int32_t);
    const static int kMinMessageLen = kHeaderLen + 2;   // nameLen + typeName
    const static int kMaxMessageLen = 64 * 1024 * 1024;
};

#endif
