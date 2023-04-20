#include <semaphore.h>

#include "Thread.h"
#include "CurrentThread.h"

namespace mymuduo {

std::atomic_int32_t Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name) 
    : started_(false),
      joined_(false),
      tid_(0),
      func_(std::move(func)),
      name_(name)
{
    setDefaultName();
}

Thread::~Thread() {
    if(started_ && !joined_) {
        // thread 类提供的设置分离线程的方法
        // Linux 中 C++11 的 thread.detach() 就相当于 C 的 pthread_detach()
        thread_->detach();
    }
}

// 一个 Thread 对象，记录的就是一个线程的详细信息
void Thread::start() {
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);

    // 开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread([&]() {
        // 获取线程的 tid 值
        tid_ = CurrentThread::tid();
        sem_post(&sem);

        // 开启一个新线程，专门执行该线程函数
        func_();
    }));

    // 因为线程间的执行顺序是不固定的，所以这里必须先等待获取上面新创建的线程的 tid 值
    sem_wait(&sem);
}

void Thread::join() {
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName() {
    int num = ++numCreated_;
    if(name_.empty()) {
        char buf[32] = {0};
        snprintf(buf, sizeof(buf), "Thread%d", num);
        name_ = buf;
    }
}

}   // namespace mymuduo