#include "Poller.h"
#include "EpollPoller.h"

#include <stdlib.h>

Poller *Poller::newDefaultPoller(EventLoop *loop) {
    if(::getenv("MUDUO_USE_POLL")) {
        // 生成 poll 实例
        return nullptr;
    } else {
        // 生成 epoll 实例
        return new EPollPoller(loop);
    }
}