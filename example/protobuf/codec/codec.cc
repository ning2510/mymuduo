#include "codec.h"
#include "mymuduo/Logger.h"

void ProtobufCodec::fillEmptyBuffer(Buffer *buf, const google::protobuf::Message &message) {
    const std::string &typeName = message.GetTypeName();
    int32_t nameLen = (int32_t)(typeName.size() + 1);
    buf->appendInt32(nameLen);
    buf->append(typeName.c_str(), nameLen);

    #if GOOGLE_PROTOBUF_VERSION > 3009002
        int byte_size = google::protobuf::internal::ToIntSize(message.ByteSizeLong());
    #else
        int byte_size = message.ByteSize();
    #endif
    buf->ensureWriteableBytes(byte_size);

    uint8_t *start = (uint8_t *)(buf->beginWirte());
    uint8_t *end = message.SerializeWithCachedSizesToArray(start);
    if(end - start != byte_size) {
        LOG_FATAL << "Protocol message was modified concurrently during serialization.";
    }
    buf->hasWritten(byte_size);

    int32_t len = ::htonl((int32_t)(buf->readableBytes()));
    buf->prepend(&len, sizeof(len));
}

void ProtobufCodec::onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp receiveTime) {
    LOG_INFO << "ProtobufCodec::onMessage receive message";
    // nameLen + typeName + kHeaderLen(len)
    while(buf->readableBytes() >= kMinMessageLen + kHeaderLen) {
        const int32_t len = buf->peekInt32();
        if(len > kMaxMessageLen || len < kMinMessageLen) {
            errorCallback_(conn, buf, receiveTime, kInvalidLength);
            break;
        } else if(buf->readableBytes() >= (size_t)(len + kHeaderLen)) {
            ErrorCode errorCode = kNoError;
            MessagePtr message = parse(buf->peek() + kHeaderLen, len, &errorCode);
            if(errorCode == kNoError && message) {
                messageCallback_(conn, message, receiveTime);
                buf->retrieve(kHeaderLen + len);
            } else {
                errorCallback_(conn, buf, receiveTime, errorCode);
                break;
            }
        } else {
            break;
        }
    }
}

google::protobuf::Message *ProtobufCodec::createMessage(const std::string &typeName) {
    google::protobuf::Message *message = nullptr;
    const google::protobuf::Descriptor *descriptor = 
        google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(typeName);
    
    if(descriptor) {
        const google::protobuf::Message *prototype =
            google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);

        if(prototype) {
            message = prototype->New();
        }
    }

    return message;
}

int toHost32(const char *buf) {
    int32_t nw32 = 0;
    ::memcpy(&nw32, buf, sizeof(nw32));
    int32_t host32 = ::ntohl(nw32);
    return host32;
}

MessagePtr ProtobufCodec::parse(const char *buf, int len, ErrorCode *error) {
    MessagePtr message;

    // get message type name
    int32_t nameLen = toHost32(buf);
    if(nameLen >= 2 && nameLen <= len - kHeaderLen) {
        std::string typeName(buf + kHeaderLen, buf + kHeaderLen + nameLen - 1);
        message.reset(createMessage(typeName));
        
        if(message) {
            const char *data = buf + kHeaderLen + nameLen;
            int dataLen = len - nameLen - kHeaderLen;
            if(message->ParseFromArray(data, dataLen)) {
                *error = kNoError;
            } else {
                *error = kParseError;
            }
        } else {
            *error = kUnknownMessageType;
        }
    } else {
        *error = kInvalidNameLen;
    }

    return message;
}

const std::string kNoErrorStr = "NoError";
const std::string kInvalidLengthStr = "InvalidLength";
const std::string kInvalidNameLenStr = "InvalidNameLen";
const std::string kUnknownMessageTypeStr = "UnknownMessageType";
const std::string kParseErrorStr = "ParseError";
const std::string kUnknownErrorStr = "UnknownError";

const std::string &ProtobufCodec::errorCodeToString(ErrorCode errorCode) {
    switch (errorCode) {
        case kNoError:
            return kNoErrorStr;
        case kInvalidLength:
            return kInvalidLengthStr;
        case kInvalidNameLen:
            return kInvalidNameLenStr;
        case kUnknownMessageType:
            return kUnknownMessageTypeStr;
        case kParseError:
            return kParseErrorStr;
        default:
            return kUnknownErrorStr;
    }
}

void ProtobufCodec::defaultErrorCallback(const TcpConnectionPtr &conn,
                                    Buffer *buf,
                                    Timestamp receiveTime,
                                    ErrorCode errorCode) 
{
    LOG_ERROR << "ProtobufCodec::defaultErrorCallback error = " << errorCodeToString(errorCode);
    if(conn && conn->connected()) {
        conn->shutdown();
    }
}