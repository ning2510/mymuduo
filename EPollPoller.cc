#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

const int kNew = -1;        // channel 未添加到 poller 中
const int kAdded = 1;       // channel 已添加到 poller 中
const int kDeleted = 2;     // channel 从 poller 中删除

EPollPoller::EPollPoller(EventLoop *loop)
     : Poller(loop), 
       epollfd_(epoll_create1(EPOLL_CLOEXEC)),
       events_(kInitEventListSize)
{
    if(epollfd_ < 0) {
        LOG_FATAL("epoll_create error: %d", errno);
    }
}

EPollPoller::~EPollPoller() {
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
    LOG_INFO("func = %s, fd total count %lu", __FUNCTION__, channels_.size());

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if(numEvents > 0) {
        LOG_INFO("%d events happened", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if(numEvents == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if(numEvents == 0) {
        LOG_DEBUG("%s timeout", __FUNCTION__);
    } else {
        if(saveErrno != EINTR) {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() error !");
        }
    }

    return now;
}

/**
 * EventLoop 包括了 ChannelList(<fd, Channel *>) 和 Poller
*/
void EPollPoller::updateChannel(Channel *channel) {
    const int index = channel->index();
    LOG_INFO("func = %s, fd = %d, events = %d, index = %d", __FUNCTION__, channel->fd(), channel->events(), index);

    // kNew：Channel 未添加到 Poller 中
    // kAdded：Channel 已经添加到 Poller 中
    // kDeleted：Channel 已经从 Poller 删除
    if(index == kNew || index == kDeleted) {
        if(index == kNew) {
            int fd = channel->fd();
            channels_[fd] = channel;
        } 
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    } else {
        // 表示 channel 已经在 poller 上注册过了
        int fd = channel->fd();
        if(channel->isNoneEvent()) {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        } else {
            update(EPOLL_CTL_MOD, channel);
        }

    }
}

// 从 poller 中删除 channel 
void EPollPoller::removeChannel(Channel *channel) {
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func = %s, fd = %d", __FUNCTION__, fd);

    int index = channel->index();
    if(index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(index);
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const {
    for(int i = 0; i < numEvents; i++) {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);

        // EventLoop 就拿到了它的 poller 给它返回的所有发生事件的 channel 列表了
        activeChannels->push_back(channel);
    }
}

// 更新 Channel 通道 epoll_ctl (add / mod / del)
void EPollPoller::update(int operation, Channel *channel) {
    epoll_event event;
    memset(&event, 0, sizeof(event));

    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;
    

    if(::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if(operation == EPOLL_CTL_DEL) {
            LOG_ERROR("epoll_ctl del error: %d", errno);
        } else {
            LOG_FATAL("epoll_ctl add/mod error: %d", errno);
        }
    }
}