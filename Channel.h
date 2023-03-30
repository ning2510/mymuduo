#ifndef _CHANNEL_H
#define _CHANNEL_H

#include "noncopyable.h"
#include "Timestamp.h"
#include <functional>
#include <memory>

class EventLoop;

/**
 * Channel 理解为通道，内部封装了 sockfd 和其感兴趣的 event，如 EPOLLIN、EPOLLOUT 事件
 * 还绑定了 poller 返回的具体事件
*/
class Channel : noncopyable {
public:
	using EventCallback = std::function<void()>;
	using ReadEventCallback = std::function<void(Timestamp)>;

	Channel(EventLoop *loop, int fd);
	~Channel();

	// fd 得到 poller 后，处理事件
	void handleEvent(Timestamp receiveTime);

	// 设置回调对象
	// cb 和 readCallback_ 都是左值，都会占用内存，因为 function 是一个对象，我们假设它占用的内存较大，所以就会浪费资源
	void setReadCallback(ReadEventCallback cb) { readCallback_ =  std::move(cb); }
	void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
	void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
	void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

	// 防止当 Channel 被手动 remove 掉，Channel 还在指向回调操作
	void tie(const std::shared_ptr<void>&);

	int fd() const { return fd_; }
	int events() const { return events_; }
	void set_revents(int revt) { revents_ = revt; }

	// 设置 fd 响应的事件状态
	void enableReading() { events_ |= kReadEvent; update(); }
	void disableReading() { events_ &= ~kReadEvent; update(); }
	void enableWriting() { events_ |= kWriteEvent; update(); }
	void disableWriting() { events_ &= ~kWriteEvent; update(); }
	void disableAll() { events_ = kNoneEvent; update(); }

	// 返回 fd 当前的事件状态
	bool isNoneEvent() const { return events_ == kNoneEvent; }
	bool isWriting() const { return events_ & kWriteEvent; }
	bool isReading() const { return events_ & kReadEvent; }

	int index() { return index_; }
	void set_index(int idx) { index_ = idx; }

	// one loop per thread
	EventLoop *ownerLoop() { return loop_; }
	void remove();


private:

	void update();
	void handleEventWithGuard(Timestamp receiveTime);

	static const int kNoneEvent;
	static const int kReadEvent;
	static const int kWriteEvent;

	EventLoop *loop_;	// 事件循环
	const int fd_;		// poller 监听的 fd
	int events_;		// 注册 fd 感兴趣的事件
	int revents_;		// poller 返回的具体发生的事件
	int index_;

	std::weak_ptr<void> tie_;
	bool tied_;

	// Channel 通道里面能够获取 fd 最终发生的具体的事件 revents，所以它负责调用具体事件的回调操作
	ReadEventCallback readCallback_;
	EventCallback writeCallback_;
	EventCallback closeCallback_;
	EventCallback errorCallback_;
};

#endif
