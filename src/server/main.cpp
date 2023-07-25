#include <iostream>
#include <signal.h>
#include "ChatServer.h"
#include "ChatService.h"

// 处理服务器`ctrl+c`结束后，重置数据库中User表的state状态信息
/* 注意：服务器可能因为很多原因导致异常退出（这里只处理了`ctrl+c`引起的异常退出问题），则需要根据实际情况，增加代码逻辑。*/
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        cerr << "command invalid example: ./ChatServer 127.0.0.1 8000" << endl;
        exit(-1);
    }
    // 解析通过命令行参数传递的ip和port
    char* ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 等待服务端ctrl+c信号的出现后，回调resetHandler，之后退出服务器
    signal(SIGINT, resetHandler);
    
    EventLoop loop;
    InetAddress listenAddr(port, ip);
    ChatServer server(&loop, listenAddr, "ChatServer");

    server.start();
    loop.loop();

    return 0;
}