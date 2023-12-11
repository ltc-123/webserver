#pragma once
#include <sys/epoll.h>
#include <sys/epoll.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include "Timer.h"

class EventLoop;
class HttpData;

class Channel {
    private:
        typedef std::function<void()> CallBack;
        EventLoop *loop_;
        int fd_;
        __uint32_t events_;//保存了fd上感兴趣的IO事件
        __uint32_t revents_;//目前fd上就绪的事件
        __uint32_t lastEvents_;
        
        //方便找到上层持有该Channel的对象
        std::weak_ptr<HttpData> holder_;

    private:
        int parse_URI();
        int parse_Headers();
        int analysisRequest();

        CallBack readHandler_;
        CallBack writeHandler_;
        CallBack errorHandler_;
        CallBack connHandler_;
    public :
        Channel(EventLoop *loop);
        Channel(EventLoop *loop, int fd);
        ~Channel();
        int getFd();
        void setFd(int fd);

        void setHolder(std::shared_ptr<HttpData> holder) { holder_ = holder; }
        std::shared_ptr<HttpData> getHolder() {
            std::shared_ptr<HttpData> ret(holder_.lock());
            return ret;
        }

        void setReadHandler(CallBack &&readHandler) { readHandler_ = readHandler; }
        void setWriteHandler(CallBack &&writeHandler) {
            writeHandler_ = writeHandler;
        }
        void setErrorHandler(CallBack &&errorHandler) {
            errorHandler_ = errorHandler;
        }
        void setConnHandler(CallBack &&connHandler) { connHandler_ = connHandler; }
        //IO事件的回调函数， EventLoop循环后，会调用POLL得到就绪事件
        //对于得到的就绪事件会依次调用此函数来处理
        void handleEvents() {
            events_ = 0;
            if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {//被挂起且没有事件可以读
                events_ = 0;
                return;
            }
            //触发错误事件
            if (revents_ & EPOLLERR) {
                if (errorHandler_) errorHandler_();
                events_ = 0;
                return;
            }
            //触发可读事件 | 高优先级可读 | 对端（客户端）关闭连接
            if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
                handleRead();
            }
            //触发可写事件
            if (revents_ & EPOLLOUT) {
                handleWrite();
            }
            //处理更新监听事件
            handleConn();
        }
        void handleRead();
        void handleWrite();
        void handleError(int fd, int err_num, std::string short_msg);
        void handleConn();

        void setRevents(__uint32_t ev) { revents_ = ev; }

        void setEvents(__uint32_t ev) { events_ = ev; }
        __uint32_t &getEvents() {return events_; }
};