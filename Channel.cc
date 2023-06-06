#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include <sys/epoll.h>

namespace mymuduo {

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd) 
    : loop_(loop), 
      fd_(fd),
      events_(0),
      revents_(0),
      index_(-1),   // kNew
      tied_(false) {}

Channel::~Channel() {}

/**
 * Channel::tie 方法什么时候会被调用：
 *      - 当一个 新连接(TcpConnection) 创建的时候
*/
void Channel::tie(const std::shared_ptr<void> &obj) {
    tie_ = obj;
    tied_ = true;
}

/* 当改变 Channel 所表示 fd 的 events 事件后，update 负责在 poller 里面更改 fd 相应的事件 epoll_ctl */
void Channel::update() {
    // 通过 Channel 所属的 EventLoop，调用 poller 的相应方法，注册 fd 的 events 事件
    loop_->updateChannel(this);
}

/* 在 Channel 所属的 EventLoop 中，把当前的 Channel 删除 */
void Channel::remove() {
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime) {
    if(tied_) {
        std::shared_ptr<void> guard = tie_.lock();
        if(guard) {
            handleEventWithGuard(receiveTime);
        }
    } else {
        handleEventWithGuard(receiveTime);
    }
}

/* 根据 poller 通知 Channel 发生的具体事件，由 Channel 负责调用具体的回调操作 */
void Channel::handleEventWithGuard(Timestamp receiveTime) {

    LOG_INFO << "Channel handleEvent revents : " << revents_;

    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if(closeCallback_) {
            closeCallback_();
        }
    }

    if(revents_ & EPOLLERR) {
        if(errorCallback_) {
            errorCallback_();
        }
    }

    if(revents_ & EPOLLIN) {
        if(readCallback_) {
            readCallback_(receiveTime);
        }
    }

    if(revents_ & EPOLLOUT) {
        if(writeCallback_) {
            writeCallback_();
        }
    }

}

}   // namespace mymuduo