#ifndef _NONCOPYABLE_H
#define _NONCOPYABLE_H

/**
 * noncopyable 被继承以后，派生类对象可以正常进行构造和析构
 * 但派生类对象无法进行拷贝构造和赋值操作
*/
class noncopyable {
public:
	noncopyable(const noncopyable&) = delete;
	noncopyable& operator=(const noncopyable&) = delete;

protected:
	noncopyable() = default;
	~noncopyable() = default;
};

#endif
