#include "Server.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <functional>
#include "Util.h"
#include "base/Logging.h"

Server::Server(EventLoop *loop, int threadNum, int port)
    :   loop_(loop),
        threadNum_(threadNum),
        eventLoopThreadPool_(new eventLoopThreadPool(loop_, threadNum)),
        started_(false),
        acceptChannel_(new Channel(loop_)),
        port_(port),
        listenFd_(socket_bind_listen(port_)) {//创建socket套接字指定端口号并监听，生成这个文件描述符
        //初始化各私有参数
    acceptChannel_->setFd(listenFd_);
    handle_for_sigpipe();
    //设为非阻塞
    if (setSocketNonBlocking(listenFd_) < 0) {
        perror("set socket non block failed");
        abort();
    }
}

void Server::start() {
    eventLoopThreadPool_->start();
    //acceptChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
    acceptChannel_->setEvents(EPOLLIN | EPOLLET);
    //设置了accpetChannel_的读回调和连接回调
    acceptChannel_->setReadHandler(bind(&Server::handNewConn, this));
    acceptChannel_->setConnHandler(bind(&Server::handThisConn, this));
    //把acceptChannel_加到事件监听器Poller中
    loop_->addToPoller(acceptChannel_, 0);
    started_ = true;
}

void Server::handNewConn() {
    //新建一个客户端地址并分配空间给它
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    socklen_t client_addr_len = sizeof(client_addr);
    int accept_fd = 0;
    while ((accept_fd = accept(listenFd_, (struct sockaddr *)&client_addr,
                                &client_addr_len)) > 0) {
        EventLoop *loop = eventLoopThreadPool_->getNextLoop();
        LOG << "New connection from " << inet_ntoa(client_addr.sin_addr) << ":"
            << ntohs(client_addr.sin_port);
        // 限制服务器的最大并发连接数
        if (accept_fd >= MAXFDS) {
            close(accept_fd);
            continue;
        }
        if (setSocketNonBlocking(accept_fd) < 0) {
            LOG << "Set non block failed";
            return ;
        }

        setSocketNodelay(accept_fd);

        shared_ptr<HttpData> req_info(new HttpData(loop, accept_fd));
        req_info->getChannel()->setHolder(req_info);
        loop->queueInLoop(std::bind(&HttpData::newEvent, req_into));
    }
    acceptChannel_->setEvents(EPOLLIN | EPOLLET);
}

