#ifndef _TIMESTAMP_H
#define _TIMESTAMP_H

#include <iostream>

namespace mymuduo {

// 时间类
class Timestamp {
public:
	Timestamp();
	explicit Timestamp(int64_t microSecondSinceEpoch);
	static Timestamp now();
	std::string toString() const;

private:
	int64_t microSecondSinceEpoch_;
};

}	// namespace mymuduo

#endif
