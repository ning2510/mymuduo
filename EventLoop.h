#ifndef _EVENTLOOP_H
#define _EVENTLOOP_H

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

#include <vector>
#include <functional>
#include <atomic>
#include <memory>
#include <mutex>

namespace mymuduo {

class Channel;
class Poller;

// 事件循环类，主要包含了俩大模块：Channel、Poller(epoll 的抽象)
class EventLoop {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    // 在当前 loop 中执行 cb
    void runInLoop(Functor cb);
    // 把 cb 放入队列中，唤醒 loop 所在的线程，执行 cb
    void queueInLoop(Functor cb);

    // 唤醒 loop 所在的线程的
    void wakeup();

    // Channel 调用 Poller 的方法，但是 Channel 不能直接和 Poller 沟通，它们中间还有一层 EventLoop
    // 所以需要借助 EventLoop 去调用 Poller 的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    // 判断 EventLoop 对象是否在自己的线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
    void abortNotInLoopThread();
    void assertInLoopThread() {
        if(!isInLoopThread()) {
            abortNotInLoopThread();
        }
    }

private:

    // 主要处理 wake up
    void handleRead();
    // 执行 pendingFunctors_ 中的回调
    void doPendingFunctors();

    using ChannelList = std::vector<Channel *>;

    std::atomic_bool looping_;      // 原子操作，通过 CAS 实现的
    std::atomic_bool quit_;          // 标识退出 loop 循环
    
    const pid_t threadId_;          // 记录当前 loop 所在线程的 ID
   
    Timestamp pollReturnTime_;      // poller 返回发生事件的 channels 的时间点
    std::unique_ptr<Poller> poller_;

    /**
     * 主要作用：当 mainLoop 获取一个新用户的 channel，通过轮询算法选择一个 subloop
     * 但 subloop 线程可能处于睡眠状态，所以通过 wakeupFd_ 唤醒 subloop 处理 channel 
    */
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    // 只记录处于活跃状态的 Channel
    ChannelList activeChannels_;

    std::atomic_bool callingPendingFunctors_;       // 标识当前 loop 是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_;          // 存储 loop 需要执行的所有的回调操作
    std::mutex mutex_;                              // 互斥锁，用来保护上面 vector 容器的线程安全操作


};

}   // namespace mymuduo

#endif
