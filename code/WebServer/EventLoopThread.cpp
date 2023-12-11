#include "EventLoopThread.h"
#include <functional>

EventLoopThread::EventLoopThread()
    :   loop_(NULL),
        exiting_(false),
        thread_(bind(&EventLoopThread::threadFunc, this), "EventLoopThread"),//添加进去
        mutex_(),
        cond_(mutex_) {}

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if (loop_ != NULL) {
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop() {
    assert(!thread_.started());
    thread_.start();//线程启动调用threadFunc函数
    {
        MutexLockGuard lock(mutex_);
        //一直等到threadFun在Thread里真正跑起来
        while (loop_ == NULL) cond_.wait();
    }
    return loop_;
}
//线程运行函数/
void EventLoopThread::threadFunc() {
    EventLoop loop;

    {
        MutexLockGuard lock(mutex_);
        loop_ = &loop;
        cond_.notify();
    }

    loop.loop();//执行该线程里的loop循环
    //assert(exiting_);
    loop_ = NULL;
}