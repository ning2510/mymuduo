#include "CurrentThread.h"

namespace CurrentThread {

    __thread int t_cachedTid = 0;

    void cacheTid() {
        if(t_cachedTid == 0) {
            // 通过 linux 系统调用，获取当前线程的 tid 值
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}