#ifndef _DISPATCHER_H
#define _DISPATCHER_H

#include "mymuduo/noncopyable.h"
#include "mymuduo/Callbacks.h"
#include "mymuduo/Timestamp.h"

#include <google/protobuf/message.h>
#include <unordered_map>
#include <memory>

using namespace mymuduo;
using MessagePtr = std::shared_ptr<google::protobuf::Message>;

class Callback : noncopyable {
public:
    virtual ~Callback() = default;
    virtual void onMessage(const TcpConnectionPtr &,
                           const MessagePtr &,
                           Timestamp) const = 0;
};

template <typename T>
class CallbackT : public Callback {
public:
    using ProtobufMessageTCallback = std::function<void(const TcpConnectionPtr &,
                                                const std::shared_ptr<T> &message,
                                                Timestamp)>;

    CallbackT(const ProtobufMessageTCallback &callback)
        : callback_(callback) {}

    void onMessage(const TcpConnectionPtr &conn,
                   const MessagePtr &message,
                   Timestamp receiveTime) const override 
    {
        std::shared_ptr<T> concrete = std::static_pointer_cast<T>(message);
        if(concrete != nullptr) {
            callback_(conn, concrete, receiveTime);
        }
    }

private:
    ProtobufMessageTCallback callback_;
};

class ProtobufDispatcher {
public:
    using ProtobufMessageCallback = std::function<void(const TcpConnectionPtr &,
                                                       const MessagePtr &,
                                                       Timestamp)>;

    explicit ProtobufDispatcher(const ProtobufMessageCallback &defaultCb)
        : defaultCallback_(defaultCb) {}

    void onProtobufMessage(const TcpConnectionPtr &conn,
                           const MessagePtr &message,
                           Timestamp receiveTime) const 
    {
        CallbackMap::const_iterator it = callbacks_.find(message->GetDescriptor());
        if(it != callbacks_.end()) {
            it->second->onMessage(conn, message, receiveTime);
        } else {
            defaultCallback_(conn, message, receiveTime);
        }
    }

    template <typename T>
    void registerMessageCallback(const typename CallbackT<T>::ProtobufMessageTCallback &cb) {
        std::shared_ptr<CallbackT<T> > callback(new CallbackT<T>(cb));
        callbacks_[T::descriptor()] = callback;
    }

private:
    using CallbackMap = std::unordered_map<const google::protobuf::Descriptor *, std::shared_ptr<Callback> >;

    CallbackMap callbacks_;
    ProtobufMessageCallback defaultCallback_;
};

#endif
