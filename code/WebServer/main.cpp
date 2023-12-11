#include <getopt.h>
#include <string>
#include "EventLoop.h"
#include "Server.h"
#include "base/Logging.h"


int main(int argc, char *argv[]) {
    int threadNum = 4;
    int port = 80;
    std::string logPath = "./WebServer.log";

    int opt;
    const char *str = "t:l:p";
    //getopt中argc和argv都由mian传入，分别表示参数的数量和参数的字符串变量数组
    //optstring是一个包含正确的参数选项字符串用于参数解析
    while ((opt = getopt(argc, argv, str)) != -1) {
        switch (opt)
        {
        case 't': {
            threadNum = atoi(optarg);
            break;
        }
        case 'l': {
            logPath = optarg;
            if (logPath.size() < 2 || optarg[0] != '/') {
                printf("logPath should start with \"/\"\n");
                abort();
            }
            break;
        }
        case 'p': {
            port = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }
    Logger::setLogFileName(logPath);
#ifndef _PTHREADS
    LOG << "_PTHREADS is not defined !";
#endif
    EventLoop mainLoop;
    Server myHTTPServer(&mainLoop, threadNum, port);
    myHTTPServer.start();
    mainLoop.loop();
    return 0;
}