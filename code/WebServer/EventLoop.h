#pragma once
#include <functional>
#include <memory>
#include <vector>
#include "Channel.h"
#include "Epoll.h"
#include "Util.h"
#include "base/CurrentThread.h"
#include "base/Logging.h"
#include "base/Thread.h"


#include <iostream>
using namespace std;

class EventLoop {
 public:
  typedef std::function<void()> Functor;
  EventLoop();
  ~EventLoop();
  void loop();
  void quit();
  void runInLoop(Functor&& cb);
  void queueInLoop(Functor&& cb);
  bool isInLoopThread() const {return threadId == CurrentThread::tid(); }
  void assertInLoopThread() { assert(isInLoopThread());}
  //只关闭连接（还可把缓冲区数据写完再关闭）
  void shutdown(shared_ptr<Channel> channel) {shutDownWR(channel->getFd()); }
  //从epoll内核时间表中删除fd及其绑定的事件
  void removeFromPoller(shared_ptr<Channel> channel) {
    //shutDownWR(channel->getFd());
    poller_->epoll_del(channel);
  }
  //把epoll内核时间表修改fd所绑定的事件
  void updatePoller(shared_ptr<Channel> channel, int timeout = 0) {
    poller_->epoll_mod(channel,timeout);
  }
  //把fd和绑定的事件注册到epoll内核事件表
  void addToPoller(shared_ptr<Channel> channel, int timeout = 0) {
    poller_->epoll_add(channel, timeout);
  }
 
 private:
  //声明顺序 wakeupFd_ > pwankeupChannnel_
  bool looping_;
  shared_ptr<Epoll> poller_;//io多路复用分发器
  int wakeupFd_;//用于异步唤醒SubLoop的Loop函数中的Poll（epoll_wait因为还没有注册fd会一直阻塞）
  bool quit_;
  bool eventHandling_;
  mutable MutexLock mutex_;
  std::vector<Functor> pendingFunctors_;//正在等待处理的函数
  bool callingPendingFunctors_;
  const pid_t threadId_;//线程id
  shared_ptr<Channel> pwakeupChannel_;//用于异步唤醒的Channel

  void wakeup();
  void handleRead();
  void doPendingFunctors();
  void handleConn();

};
