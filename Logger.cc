#include "Logger.h"

#include <unistd.h>
#include <syscall.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

namespace mymuduo {

static pid_t g_pid = 0;
static thread_local pid_t g_tid = 0;

static mymuduo::Logger::ptr g_logger;

void initLog(const char *file_name, const char *file_path /*= "./"*/, int max_size /*= 5 MB*/, 
             int sync_interval /*= 500 ms*/, LogLevel level /*= DEBUG*/) {
    
    if(g_logger) return ;
    
    g_logger = std::make_shared<Logger>();
    setLogLevel(level);
    g_logger->init(file_name, file_path, max_size, sync_interval);
    g_logger->start();
}

void CoredumpHandler(int signal_no) {
    LOG_ERROR << "progress received invalid signal, will exit";
    
    g_logger->stop();
    g_logger->getAsyncLogger()->join();

    ::signal(signal_no, SIG_DFL);
    ::raise(signal_no);
}

pid_t getpid() {
    if(g_pid == 0) {
        g_pid = ::getpid();
    }
    return g_pid;
}

pid_t gettid() {
    if(g_tid == 0) {
        g_tid = ::syscall(SYS_gettid);
    }
    return g_tid;
}

bool openLog() {
    if(!g_logger) return false;
    return true;
}

void setLogLevel(LogLevel level) {
    g_log_level = level;
}

LogLevel stringToLevel(const std::string &str) {
    if(str == "DEBUG") return LogLevel::DEBUG;
    if(str == "INFO") return LogLevel::INFO;
    if(str == "WARN") return LogLevel::WARN;
    if(str == "ERROR") return LogLevel::ERROR;
    if(str == "FATAL") return LogLevel::FATAL;
    return LogLevel::NONE;
}

std::string levelToString(LogLevel level) {
    std::string str = "DEBUG";

    switch (level) {
        case LogLevel::DEBUG:
            str = "DEBUG";
            break;
        case LogLevel::INFO:
            str = "INFO";
            break;
        case LogLevel::WARN:
            str = "WARN";
            break;
        case LogLevel::ERROR:
            str = "ERROR";
            break;
        case LogLevel::FATAL:
            str = "FATAL";
        default:
            str = "NONE";
            break;
    }

    return str;
}

// LogEvent ---------------------------------------------- 
LogEvent::LogEvent(LogLevel level, const char *file_name, int line, const char *func_name)
    : level_(level),
      pid_(0),
      tid_(0),
      file_name_(file_name),
      line_(line),
      func_name_(func_name) {}

void LogEvent::log() {
    ss_ << "\n";
    if(level_ >= g_log_level) {
        g_logger->push(ss_.str());
    }
}

std::stringstream &LogEvent::getStringStream() {
    ::gettimeofday(&timeval_, nullptr);

    struct tm time;
    ::localtime_r(&(timeval_.tv_sec), &time);

    const char *format = "%Y-%m-%d %H:%M:%S";
    char buf[128];
    ::strftime(buf, sizeof(buf), format, &time);
    ss_ << "[" << buf << "." << timeval_.tv_usec << "]\t";

    std::string s_level = levelToString(level_);
    ss_ << "[" << s_level << "]\t";

    pid_ = getpid();
    tid_ = gettid();

    ss_ << "[" << pid_ << "]\t[" << tid_ << "]\t[" 
         << file_name_ << ":" << line_ << "]\t";

    return ss_;
}


// AsyncLogger ---------------------------------------------- 

AsyncLogger::AsyncLogger(const char *file_name, const char *file_path, int max_size)
    : file_name_(file_name),
      file_path_(file_path),
      max_size_(max_size),
      no_(0),
      stop_(false),
      need_reopen_(false),
      file_handle_(nullptr) {

    int rt = sem_init(&sem_, 0, 0);
    assert(rt == 0);

    rt = ::pthread_mutex_init(&mutex_, nullptr);
    assert(rt == 0);

    rt = ::pthread_create(&thread_, nullptr, &AsyncLogger::execute, this);
    assert(rt == 0);

    rt = ::sem_wait(&sem_);
    assert(rt == 0);

    sem_destroy(&sem_);
}

void AsyncLogger::push(std::vector<std::string> &buffer) {
    if(buffer.empty()) return ;

    ::pthread_mutex_lock(&mutex_);
    m_tasks.push(buffer);
    ::pthread_mutex_unlock(&mutex_);

    ::pthread_cond_signal(&cond_);
}

void AsyncLogger::flush() {
    if(file_handle_) {
        ::fflush(file_handle_);
    }
}

void AsyncLogger::stop() {
    if(!stop_) {
        stop_ = true;
        ::pthread_cond_signal(&cond_);
    }
}

void AsyncLogger::join() {
    ::pthread_join(thread_, nullptr);
}

void *AsyncLogger::execute(void *arg) {
    AsyncLogger *ptr = reinterpret_cast<AsyncLogger *>(arg);
    int rt = ::pthread_cond_init(&ptr->cond_, nullptr);
    assert(rt == 0);

    rt = ::sem_post(&ptr->sem_);
    assert(rt == 0);

    while(1) {
        ::pthread_mutex_lock(&ptr->mutex_);

        while(ptr->m_tasks.empty() && !ptr->stop_) {
            ::pthread_cond_wait(&ptr->cond_, &ptr->mutex_);
        }
        bool is_stop = ptr->stop_;
        if(is_stop && ptr->m_tasks.empty()) {
            ::pthread_mutex_unlock(&ptr->mutex_);
            break;
        }

        std::vector <std::string> tmp;
        tmp.swap(ptr->m_tasks.front());
        ptr->m_tasks.pop();

        ::pthread_mutex_unlock(&ptr->mutex_);

        timeval now;
        ::gettimeofday(&now, nullptr);

        struct tm now_time;
        ::localtime_r(&(now.tv_sec), &now_time);

        const char *format = "%Y%m%d";
        char date[32];
        ::strftime(date, sizeof(date), format, &now_time);
        if(ptr->date_ != std::string(date)) {
            ptr->no_ = 0;
            ptr->date_ = std::string(date);
            ptr->need_reopen_ = true;
        }

        if(!ptr->file_handle_) {
            ptr->need_reopen_ = true;
        }

        std::stringstream ss;
        ss << ptr->file_path_ << ptr->file_name_ << "_" 
           << ptr->date_ << "_" << ptr->no_ << ".log";
        std::string full_file_name = ss.str();

        if(ptr->need_reopen_) {
            if(ptr->file_handle_) {
                ::fclose(ptr->file_handle_);
            }

            ptr->file_handle_ = ::fopen(full_file_name.c_str(), "a");
            if(ptr->file_handle_ == nullptr) {
                printf("open fail errno = %d reason = %s \n", errno, strerror(errno));
                exit(-1);
            }
            ptr->need_reopen_ = false;
        }

        if(::ftell(ptr->file_handle_) > ptr->max_size_) {
            ::fclose(ptr->file_handle_);

            ptr->no_++;
            std::stringstream ss2;
            ss2 << ptr->file_path_ << ptr->file_name_ << "_" 
               << ptr->date_ << "_" << ptr->no_ << ".log";
            full_file_name = ss2.str();
        
            ptr->file_handle_ = ::fopen(full_file_name.c_str(), "a");
            if(ptr->file_handle_ == nullptr) {
                printf("open fail errno = %d reason = %s \n", errno, strerror(errno));
                exit(-1);
            }
            ptr->need_reopen_ = false;
        }

        for(auto s : tmp) {
            if(!s.empty()) {
                ::fwrite(s.c_str(), 1, s.length(), ptr->file_handle_);
            }
        }
        tmp.clear();
        ::fflush(ptr->file_handle_);

        if(is_stop) break;
    }
    
    if (ptr->file_handle_) {
        ::fclose(ptr->file_handle_);
        ptr->file_handle_ = nullptr;
    }

    return nullptr;
}


// Logger ---------------------------------------------- 
void Logger::init(const char *file_name, const char *file_path, 
                  int max_size, int sync_interval) {
    
    if(!is_init_) {
        sync_interval_ = sync_interval;
        async_logger_ = std::make_shared<AsyncLogger>(file_name, file_path, max_size);

        ::signal(SIGSEGV, CoredumpHandler);
        ::signal(SIGABRT, CoredumpHandler);
        ::signal(SIGTERM, CoredumpHandler);
        ::signal(SIGKILL, CoredumpHandler);
        ::signal(SIGINT, CoredumpHandler);
        ::signal(SIGSTKFLT, CoredumpHandler);
        ::signal(SIGPIPE, SIG_IGN);   /* ignore */

        is_init_ = true;
    }
}

void *Logger::collect(void *arg) {
    Logger* ptr = reinterpret_cast<Logger*>(arg);
    ptr->stop_ = false;
    __useconds_t s_time = ptr->sync_interval_ * 1000;

    while(!ptr->stop_) {
        ::usleep(s_time);
        ptr->loopFunc();
    }

    return nullptr;
}

void Logger::start() {
    /* tmp method */
    int rt = ::pthread_create(&thread_, nullptr, &Logger::collect, this);
    assert(rt == 0);
}

void Logger::stop() {
    /* tmp method */
    stop_ = true;

    loopFunc();
    join();
    async_logger_->stop();
    async_logger_->flush();
}

void Logger::loopFunc() {
    std::vector<std::string> tmp;

    ::pthread_mutex_lock(&mutex_);
    tmp.swap(buffer_);
    ::pthread_mutex_unlock(&mutex_);

    async_logger_->push(tmp);
}

void Logger::push(const std::string &msg) {
    ::pthread_mutex_lock(&mutex_);

    buffer_.push_back(std::move(msg));
    
    ::pthread_mutex_unlock(&mutex_);
}

void Exit(int code) {
    LOG_INFO << "logger will exit";

    g_logger->stop();
    g_logger->getAsyncLogger()->join();

    _exit(code);
}

}   // namespace mymuduo
