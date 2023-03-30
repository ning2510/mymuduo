#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      callback_(cb) {

}

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if(loop_ != nullptr) {
        loop_->quit();
        thread_.join();
    }
}

EventLoop *EventLoopThread::startLoop() {
    thread_.start();    // 启动底层的新线程，该线程负责执行 EventLoopThread::threadFunc()

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_ == nullptr) {
            /**
             * 先unlock之前获得的 mutex，然后阻塞当前的执行线程。把当前线程添加到等待线程列表中，
             * 该线程会持续 block 直到被 notify_all() 或 notify_one() 唤醒。被唤醒后，该 thread 
             * 会重新获取 mutex，获取到mutex后执行后面的动作
            */
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}

// threadFunc 是在一个单独的新线程中运行的
void EventLoopThread::threadFunc() {
    EventLoop loop;     // 创建一个独立的 EventLoop，和上面的线程是一一对应的 (one loop per thread)

    if(callback_) {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();

    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}