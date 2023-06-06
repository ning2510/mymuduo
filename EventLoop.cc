#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

namespace mymuduo {

// 防止一个线程创建多个 EventLoop (thread_local 的机制)
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的 Poller IO 复用接口的超时时间
const int kPollTimeMs = 10000;

// 创建 wakeupFd，用来 notify 唤醒 subReactor 处理新来的 channel
int createEventfd() {
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtfd < 0) {
        LOG_FATAL << "eventfd error : " << errno;
    }
    return evtfd;
}

EventLoop::EventLoop() 
    : looping_(false),
      quit_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)) {

    LOG_DEBUG << "EventLoop created [" << this << "] in thread " << threadId_;
    if(t_loopInThisThread) {
        LOG_FATAL << "Anthor EventLoop [" << this <<  "] exist in this thread " << threadId_; 
    } else {
        t_loopInThisThread = this;
    }

    // 设置 wakeupFd_ 的事件类型，以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个 EventLoop 都将监听 wakeupChannel_ 的 EPOLLIN 读事件了
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop() {
    looping_ = true;
    quit_ = false;

    LOG_INFO << "EventLoop [" << this << "] start looping";

    while(!quit_) {
        activeChannels_.clear();
        // 这里 epoll_wait 主要监听两类 fd：一种是和客户端通信的 fd，另一种就是 subLoop 和 mainLoop 通信的 wakeupFd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for(Channel *channel : activeChannels_) {
            // Poller 监听到哪些 channel 发生事件了，然后就上报给 EventLoop，通知 channel 处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前 EventLoop 事件循环需要处理的回调操作
        /**
         * IO 线程 mainLoop 主要做 accept 操作，而 accept 会返回 fd 后会被包装到 channel 中交给 subLoop
         * mainLoop 会事先注册一个回调 cb，需要 subLoop 来执行这个 cb
         * 当 mainLoop 通过 wakeupFd 唤醒 subLoop 后，subLoop 就需要通过 doPendingFunctors() 方法执行相应的回调
         * 而这些回调是 mainLoop 事先放到 pendingFunctors_ 中的
        */
        doPendingFunctors();
    }

    LOG_INFO << "EventLoop [" << this << "] start looping";
}

/**
 * 退出事件循环，有两种情况：
 *      1. loop 在自己的线程中调用 quit
 *      2. 在其他 loop 线程中，调用 loop 的 quit 
*/
void EventLoop::quit() {
    quit_ = true;

    // 假如在一个 subLoop 中调用 mainLoop 的 quit
    // 那么就需要先把 mainLoop 唤醒，因为此时 mainLoop 可能处于睡眠状态 (因为 epoll_wait) 
    if(!isInLoopThread()) {
        wakeup();
    }
}

// 在当前 loop 中执行 cb
void EventLoop::runInLoop(Functor cb) {
    if(isInLoopThread()) {  // 在当前的 loop 线程中，执行 cb
        cb();
    } else {    // 在非当前 loop 线程中执行 cb()，就需要唤醒 loop 所在线程执行 cb
        queueInLoop(cb);
    }
}

// 把 cb 放入队列中，唤醒 loop 所在的线程，执行 cb
void EventLoop::queueInLoop(Functor cb) {
    {
        // 智能锁
        std::unique_lock<std::mutex> lock(mutex_);
        // push_back() 是拷贝构造，emplace_back() 是直接构造
        pendingFunctors_.emplace_back(cb);
    }

    /**
     * - 唤醒相应的需要执行上面回调操作的 loop 线程
     * - callingPendingFunctors_ 为 true 说明，当前 loop 正在执行回调，还没有执行完。但是 loop 又有新的回调了，
     * 而执行完回调会可能又会阻塞到 epoll_wait，所以此时需要再次唤醒，让 loop 继续执行新来的回调
    */
    if(!isInLoopThread() || callingPendingFunctors_) {
        wakeup();       // 唤醒 loop 所在线程
    }
}

void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)) {
        LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
}

// 唤醒 loop 所在的线程的
void EventLoop::wakeup() {
    // 向 wakeupFd_ 写一个数据，wakeupChannel 就会发生都事件，当前的 loop 线程就会被唤醒
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)) {
        LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
}

// Channel 调用 Poller 的方法，但是 Channel 不能直接和 Poller 沟通，它们中间还有一层 EventLoop
// 所以需要借助 EventLoop 去调用 Poller 的方法
void EventLoop::updateChannel(Channel *channel) {
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel) {
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel) {
    return poller_->hasChannel(channel);
}

// 执行 pendingFunctors_ 中的回调
void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(const Functor &functor : functors) {
        // 执行当前 loop 需要执行的回调操作
        functor();
    }

    callingPendingFunctors_ = false;
}

void EventLoop::abortNotInLoopThread() {
    LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop[" << this << "], its threadId_ = " 
              << threadId_ << ", current thread id = " << CurrentThread::tid();
}

}   // namespace mymuduo