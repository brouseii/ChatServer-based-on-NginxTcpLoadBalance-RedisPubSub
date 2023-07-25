#include "ChatServer.h"
#include "json.hpp"
using json = nlohmann::json;
#include "muduo/base/Logging.h"

#include "ChatService.h"
#include <functional>
#include <string>
#include <iostream>
using namespace std;
using namespace placeholders;


// 初始化聊天服务器对象
ChatServer::ChatServer(EventLoop* loop, const InetAddress& listenAddr, const string& nameArg)
    : _loop(loop)
    , _server(loop, listenAddr, nameArg)
{   
    // 注册连接回调函数
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));
    
    // 注册消息回调函数
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    // 设置线程数量
    _server.setThreadNum(4);  // 1个main reactor、3个sub reactor
}

// 启动服务
void ChatServer::start()
{
    _server.start();
}

// 上报连接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr& conn)
{
    // 客户端断开连接
    if (!conn->connected())
    {
        // 通过ChatService中的静态成员instance获取其单例对象实例，并调用其clientCloseException()成员函数
        //，用来处理客户端连接断开后，将其上的用户连接在数据库中的状态改为offline
        ChatService::instance()->clientCloseException(conn); 
        conn->shutdown();
    }
}

// 上报读写事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
{
    // 从buf中，获取json字符串
    string buf_ = buf->retrieveAllAsString(); 
    cout << "Execute: " << buf << endl;

    // 将buf中获取到的json字符串数据反序列化
    json js = json::parse(buf_); 
  
    // 目的：完全解耦网络模块的代码和业务模块的代码
    // 通过js["msgid"]获取msgHandler ==> msgHandler(conn, js, time)
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    // 回调消息绑定好的事件处理器，来执行相应的业务处理
    msgHandler(conn, js, time);
}
