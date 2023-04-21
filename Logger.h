#ifndef _LOG_H
#define _LOG_H

#include <sstream>
#include <string>
#include <memory>
#include <time.h>
#include <sys/time.h>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <semaphore.h>

namespace mymuduo {

enum LogLevel {
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
	FATAL = 5,
    NONE = 6
};

static LogLevel g_log_level = DEBUG;

#define LOG_DEBUG \
    if(mymuduo::openLog() && mymuduo::LogLevel::DEBUG >= mymuduo::g_log_level) \
        mymuduo::LogTmp(mymuduo::LogEvent::ptr(new mymuduo::LogEvent(mymuduo::LogLevel::DEBUG, __FILE__, __LINE__, __func__))).getStringStream()

#define LOG_INFO \
    if(mymuduo::openLog() && mymuduo::LogLevel::INFO >= mymuduo::g_log_level)  \
        mymuduo::LogTmp(mymuduo::LogEvent::ptr(new mymuduo::LogEvent(mymuduo::LogLevel::INFO, __FILE__, __LINE__, __func__))).getStringStream()

#define LOG_WARN \
    if(mymuduo::openLog() && mymuduo::LogLevel::WARN >= mymuduo::g_log_level)  \
        mymuduo::LogTmp(mymuduo::LogEvent::ptr(new mymuduo::LogEvent(mymuduo::LogLevel::WARN, __FILE__, __LINE__, __func__))).getStringStream()

#define LOG_ERROR \
    if(mymuduo::openLog() && mymuduo::LogLevel::ERROR >= mymuduo::g_log_level) \
        mymuduo::LogTmp(mymuduo::LogEvent::ptr(new mymuduo::LogEvent(mymuduo::LogLevel::ERROR, __FILE__, __LINE__, __func__))).getStringStream()

#define LOG_FATAL \
    if(mymuduo::openLog() && mymuduo::LogLevel::FATAL >= mymuduo::g_log_level) \
        mymuduo::LogTmp(mymuduo::LogEvent::ptr(new mymuduo::LogEvent(mymuduo::LogLevel::FATAL, __FILE__, __LINE__, __func__))).getStringStream()


/* 5MB 500ms */
void initLog(const char *file_name, const char *file_path = "./", int max_size = 5 * 1024 * 1024, int sync_interval = 500, LogLevel level = DEBUG);

pid_t getpid();
pid_t gettid();
bool openLog();

void setLogLevel(LogLevel level);
LogLevel stringToLevel(const std::string &str);
std::string levelToString(LogLevel level);

class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> ptr;

    LogEvent(LogLevel level, const char *file_name, int line, const char *func_name);
    ~LogEvent() {}

    void log();
    std::stringstream &getStringStream();
    LogLevel level_;
private:
    timeval timeval_;
    

    pid_t pid_;
    pid_t tid_;

    int line_;
    std::string file_name_;
    std::string func_name_;

    std::stringstream ss_;
};


class LogTmp {
public:
    explicit LogTmp(LogEvent::ptr event) : event_(event) {}
    ~LogTmp() {
        event_->log();
		if(event_->level_ == LogLevel::FATAL) exit(0);
    }

    std::stringstream &getStringStream() {
        return event_->getStringStream();
    }

private:
    LogEvent::ptr event_;
};


class AsyncLogger {
public:
    typedef std::shared_ptr<AsyncLogger> ptr;

    AsyncLogger(const char *file_name, const char *file_path, int max_size);
    ~AsyncLogger() {
        pthread_cond_destroy(&cond_);
        pthread_mutex_destroy(&mutex_);
    }

    void push(std::vector<std::string> &buffer);
    void flush();
    void stop();
    void join();

    static void *execute(void *arg);

private:
    std::string file_name_;
    std::string file_path_;
    std::string date_;

    int max_size_;
    int no_;

    bool stop_;
    bool need_reopen_;
    FILE *file_handle_;

    sem_t sem_;
    pthread_mutex_t mutex_;
    pthread_t thread_;
    pthread_cond_t cond_;
    
    std::queue <std::vector<std::string>> m_tasks;
};


class Logger {
public:
    typedef std::shared_ptr<Logger> ptr;

    Logger() : is_init_(false), stop_(false) {
        ::pthread_mutex_init(&mutex_, nullptr);
    }
    ~Logger() {
        stop();
        async_logger_->join();
        ::pthread_mutex_destroy(&mutex_);
    }

    void init(const char *file_name, const char *file_path, 
              int max_size, int sync_interval);

    
    /* tmp method */
    static void *collect(void *arg);
    void join() {
        ::pthread_join(thread_, nullptr);
    }

    void start();
    void stop();
    void loopFunc();
    void push(const std::string &msg);

    AsyncLogger::ptr getAsyncLogger() {
        return async_logger_;
    }

    std::vector<std::string> buffer_;

private:
    /* tmp method */
    bool stop_;
    pthread_t thread_;

    pthread_mutex_t mutex_;
    bool is_init_;
    int sync_interval_;

    AsyncLogger::ptr async_logger_;
};

void Exit(int code);

}   // namespace mymuduo

#endif
