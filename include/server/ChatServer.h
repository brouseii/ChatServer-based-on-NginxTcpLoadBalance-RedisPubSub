#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
using namespace muduo;
using namespace muduo::net;

// 聊天服务器的主类
class ChatServer
{
public:
    // 初始化聊天服务器对象
    ChatServer(EventLoop* loop, const InetAddress& listenAddr, const string& nameArg);
    // 启动服务
    void start();
private:
    // 上报连接相关信息的回调函数
    void onConnection(const TcpConnectionPtr& conn);
    // 上报读写事件相关信息的回调函数
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time);

    // 组合的muduo库，实现服务器功能的类对象
    TcpServer _server;
    // 指向事件循环的指针
    EventLoop* _loop; 
};


#endif