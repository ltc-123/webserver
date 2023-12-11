#include "EventLoop.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <iostream>
#include "Util.h"
#include "base/Logging.h"

using namespace std;

__thread EventLoop* t_loopInThisThread = 0;
//创建eventfd 类似管道的进程间通信方式
int createEventfd() {
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);//创建为非阻塞的文件描述符
    if (evtfd < 0) {
        LOG << "Failed in eventfd";
        abort();
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false),
      poller_(new Epoll()),
      wakeupFd_(createEventfd()),
      quit_(false),
      eventHandling_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      pwakeupChannel_(new Channel(this, wakeupFd_)) {
  if (t_loopInThisThread) {

  } else {
    t_loopInThisThread = this;
  }
  pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);
  pwakeupChannel_->setReadHandler(bind(&EventLoop::handleRead, this));
  pwakeupChannel_->setConnHandler(bind(&EventLoop::handleConn, this));
  poller_->epoll_add(pwakeupChannel_, 0);
}

void EventLoop::handleConn() {
    updatePoller(pwakeupChannel_, 0);
}
//析构函数
EventLoop::~EventLoop() {
    close(wakeupFd_);
    t_loopInThisThread = NULL;
}
//异步唤醒SubLoop的epoll_wait(向event_fd中写入数据)
void EventLoop::wakeup() {
    uint64_t one = 1;
    //直到齐了才返回
    ssize_t n = writen(wakeupFd_, (char*)(&one), sizeof one);
    if (n != sizeof one) {
        LOG << "EventLoop::wakeup() writes " << n << "bytes instead of 8";
    }
}
//eventfd的读回调函数(因为event_fd写了数据，所以触发可读事件，从event_fd读数据)
void EventLoop::handleRead() {
    uint64_t one = 1;
    //直到齐了才返回
    ssize_t n = readn(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        LOG << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
    pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);
}
//如果当前线程就是创建EventLoop的线程，那么调用callback（关闭EpollDel）否则放入等待执行函数区
void EventLoop::runInLoop(Functor&& cb) {
    if (isInLoopThread())
        cb();
    else   
        queueInLoop(std::move(cb));
}
//把此函数放入等待执行函数区，如果当前是跨线程或者正在调用等待的函数，则唤醒
void EventLoop::queueInLoop(Functor&& cb) {
    {
        MutexLockGuard lock(mutex_);
        pendingFunctors_.emplace_back(std::move(cb));//放入容器中
    }

    if (!isInLoopThread() || callingPendingFunctors_)
        wakeup();
}

void EventLoop:loop() {
    assert(!looping_);
    assert(isInLoopThread());
    looping_ = true;
    quit_ = false;
    std::vector<SP_Channel> ret;
    while (!quit_) {
        ret.clear();
        ret = poller_->poll();
        eventHandling_ = true;
        for (auto& it : ret) it->handleEvents();
        eventHanding_ = false;
        doPendingFunctors();//执行额外的函数
        poller_handleExpired();//执行超时的回调函数
    }
    looping_ = false;
}
//执行额外的函数（subLoop注册EpollADD连接套接字以及绑定事件的函数）
void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        MutexLockGuard lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (size_t i = 0; i < functors.size(); ++i) functors[i]();
    callingPendingFunctors_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    if (!isInLoopThread()) {
        wakeup();
    }
}