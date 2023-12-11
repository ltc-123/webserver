#pragma once
#include "EventLoop.h"
#include "base/Condition.h"
#include "base/MutexLock.h"
#include "base/Thread.h"
#include "base/noncopyable.h"

class EventLoopThread : noncopyable {//noncopyable实现不可拷贝
    public:
        EventLoopThread();
        ~EventLoopThread();
        EventLoop* startLoop();

    private:
        void threadFunc();
        EventLoop* loop_;
        bool exiting_;
        Thread thread_;
        MutextLock mutex_;
        Condition cond_;
};