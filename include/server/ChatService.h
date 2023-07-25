#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include "json.hpp"
using json = nlohmann::json;

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include <mutex>

#include "UserModel.h"
#include "OfflineMsgModel.h"
#include "FriendModel.h"
#include "GroupModel.h"

#include "redis.h"

using namespace std;
using namespace muduo;
using namespace muduo::net;

// 表示处理消息的事件回调方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr&, json&, Timestamp)>;

// 聊天服务器业务类：
class ChatService
{
public: 
    // 静态成员函数：获取ChatService的单例对象
    static ChatService* instance();

    // 服务器异常，业务重置方法
    void reset(); 

    // 处理客户端（因发送了不正常的消息）异常退出：在_userConnMap中删除conn连接，并将该连接的用户在数据库中的状态变为offline
    void clientCloseException(const TcpConnectionPtr& conn);

    // 获取消息对应的处理器
    MsgHandler getHandler(int msgid);

    // 处理登录业务
    void login(const TcpConnectionPtr&, const json&, Timestamp);
    // 处理注销业务
    void loginout(const TcpConnectionPtr&, const json&, Timestamp);

    // 处理注册业务
    void signup(const TcpConnectionPtr&, const json&, Timestamp);

    // 一对一聊天业务
    void oneChat(const TcpConnectionPtr&, const json&, Timestamp);

    // 添加好友业务
    bool addFriend(const TcpConnectionPtr&, const json&, Timestamp);

    // 创建群组业务
    bool createGroup(const TcpConnectionPtr&, const json&, Timestamp);
    // 加入群组业务
    bool addGroup(const TcpConnectionPtr&, const json&, Timestamp);
    // 群组聊天业务
    void groupChat(const TcpConnectionPtr&, const json&, Timestamp);

    // 从redis消息队列中获取订阅的消息
    void handleRedisSubscribeMessage(int, string);
private:
    ChatService();

    // 存储消息id和其对应的业务处理方法
    unordered_map<int, MsgHandler> _msgHandlerMap;

    // 存储在线用户的通信连接<userid, TcpConnectionPtr>
    unordered_map<int, TcpConnectionPtr> _userConnMap;
    // 定义互斥锁，保证_userConnMap的线程安全
    mutex _connMutex;

    // 数据操作类对象
    UserModel _userModel; 
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;

    // redis操作对象
    Redis _redis;
};

#endif