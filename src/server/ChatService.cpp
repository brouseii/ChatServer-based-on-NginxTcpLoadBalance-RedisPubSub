#include "ChatService.h"
#include "public.h"
#include <muduo/base/Logging.h> 

// 获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}

// _msgHandlerMap中，保存<“注册消息”、“对应的Handler回调操作”>
ChatService::ChatService()
{ 
    // 用户基本业务管理相关事件处理回调注册
    _msgHandlerMap[LOGIN_MSG] = std::bind(&ChatService::login, this, _1, _2, _3);
    _msgHandlerMap[LOGINOUT_MSG] = std::bind(&ChatService::loginout, this, _1, _2, _3);
    _msgHandlerMap[SIGNUP_MSG] = std::bind(&ChatService::signup, this, _1, _2, _3);
    _msgHandlerMap[ONE_CHAT_MSG] = std::bind(&ChatService::oneChat, this, _1, _2, _3);
    _msgHandlerMap[ADD_FRIEND_MSG] = std::bind(&ChatService::addFriend, this, _1, _2, _3);

    // 群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接redis服务器
    if (_redis.connect())
    {
        // 设置上报消息回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

// 服务器异常，业务重置方法
void ChatService::reset()
{
    // 将数据库中User表里，state=online状态的用户设置成offline
    _userModel.resetState();
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    // msgid没有对应的事件处理回调，则返回一个默认的处理器（其中执行空操作）
    if (_msgHandlerMap.find(msgid) == _msgHandlerMap.end())
    {
        return [=](const TcpConnectionPtr& conn, json& js, Timestamp time) {
            LOG_ERROR << "msgid : " << msgid << "can not find handler!";
        };
    }

    return _msgHandlerMap[msgid];
}

// 处理客户端异常退出：在_userConnMap中删除conn连接，并将该连接的用户在数据库中的状态变为offline
void ChatService::clientCloseException(const TcpConnectionPtr& conn)
{ 
    User user;  
    {
        // 存储在线用户通信连接的信息，保证其线程安全
        lock_guard<mutex> lock(_connMutex);   // 出作用域后，自动释放lock占用的资源（保证锁的力度尽可能的小）

        // 遍历userConnMap_，从_userConnMap中删除conn的连接信息 
        for (auto iter = _userConnMap.begin(); iter != _userConnMap.end(); ++iter)
        {
            if (iter->second == conn)
            {
                user.setID(iter->first);   // 初始化user的userid
                _userConnMap.erase(iter);
                break;
            }
        } 
    }

    // 用户注销，相当于下线，在redis中取消订阅通道
    _redis.unsubscribe(user.getID());

    if (user.getID() != -1)
    {
        // 更新该userid用户在数据库中的状态信息
        user.setState("offline");
        _userModel.updateState(user); 
    } 
}

// 处理登录业务
void ChatService::login(const TcpConnectionPtr& conn, const json& js, Timestamp time)
{ 
    int id = js["id"].get<int>(); 

    User user = _userModel.query(id); 

    if (user.getID() == id && user.getPwd() == js["password"])
    {
        if (user.getState() == "online") {
            /* 该用户已经处于登录状态，不允许重复登录 */
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2; 
            response["errmsg"] = "the user is already online!!!";

            // 将json对象序列化为字符串，并发送该消息
            conn->send(response.dump());
        } else {
            /* 登录成功，记录用户的连接信息 */
            {
                // 存储在线用户通信连接的信息，并保证其线程安全
                lock_guard<mutex> lock(_connMutex);   // 出作用域后，自动释放lock占用的资源（保证锁的力度尽可能的小）
                _userConnMap.insert({id, conn});
            }  

            // userid=id的用户登录成功后，向redis订阅channel的消息，其channel管道名为id（即自己的userid，便于跨服务器通信时其他服务器上的用户能通过redis中间件找到自己）
            if (!_redis.subscribe(id))
            {
                LOG_ERROR << "[" << id << "] subscribe redis failure!";
            }

            // 1.登录成功后，更新用户状态信息 state : offline ==> online到数据库中
            user.setState("online");
            if (!_userModel.updateState(user))
            {
                LOG_INFO << "update state failure!!!"; 
                exit(-1);
            }  

            /* 开始打包要向客户端发送的首次登录成功后的json返回消息 */
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getID();
            response["name"] = user.getName();  

            // 2.登录成功后，查询该用户是否有离线消息，有则将其加入json返回消息中
            vector<string> offlineMsgs = _offlineMsgModel.query(id);
            if (!offlineMsgs.empty())
            {
                response["offlineMsg"] = offlineMsgs;
                // 该用户读取离线消息后，将所有离线消息删除掉
                _offlineMsgModel.remove(id);
            } 

            // 3.查询用户的好友信息，有则将其加入json返回消息中
            vector<User> friendUsers = _friendModel.query(id);
            if (!friendUsers.empty())
            {
                vector<string> friendStrings;
                for (User& friendUser : friendUsers)
                {
                    json js; 
                    js["id"] = (int)friendUser.getID();
                    js["name"] = (string)friendUser.getName();
                    js["state"] = (string)friendUser.getState();
                    friendStrings.push_back(js.dump());    // 将json对象序列化为字符串
                } 
                response["friends"] = friendStrings;
            } 

            // 4.查询用户的群组信息，有则将其加入json返回消息中
            vector<Group> groups = _groupModel.queryGroups(id);
            if (!groups.empty())
            {
                vector<string> groupStrs;
                for (Group& group : groups)
                { 
                    json groupJson;
                    groupJson["id"] = group.getID();
                    groupJson["groupname"] = group.getName();
                    groupJson["groupdesc"] = group.getDesc();
                    vector<string> groupUsers;
                    for (GroupUser& user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getID();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        groupUsers.push_back(js.dump());    // 将json对象序列化为字符串
                    }
                    groupJson["users"] = groupUsers;
                    groupStrs.push_back(groupJson.dump());  // 将json对象序列化为字符串
                }
                response["groups"] = groupStrs;
            } 
 
            // 5.将json对象序列化为字符串，并发送该消息
            conn->send(response.dump());
        }
    }
    else
    {
        /* 用户不存在，即登录失败 */  
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1; 
        response["errmsg"] = "user's id or password is error!!!";

        // 将json对象序列化为字符串，并发送该消息
        conn->send(response.dump());
    } 
}

// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr& conn, const json& js, Timestamp time)
{
    int userid = js["id"].get<int>();

    // 删除userid在_userConnMap中的连接
    {
        lock_guard<mutex> lock(_connMutex);  // 保证_userConnMap的线程安全
        auto iter = _userConnMap.find(userid);
        if (iter != _userConnMap.end())
        {
            _userConnMap.erase(iter);
        }
    }

    // 用户注销，相当于下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);

    // 更新userid在数据库User表中的状态
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}

// 处理注册业务
void ChatService::signup(const TcpConnectionPtr& conn, const json& js, Timestamp time)
{ 
    User user;  // user的默认状态state为offline
    user.setName(js["name"]);
    user.setPwd(js["password"]); 
    if (_userModel.insert(user))
    {
        /* 注册成功 */ 
        json response;
        response["msgid"] = SIGNUP_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getID();

        // 将json对象序列化为字符串，并发送该消息
        conn->send(response.dump());
    }
    else
    {
        /* 注册失败 */ 
        json response;
        response["msgid"] = SIGNUP_MSG_ACK;
        response["errno"] = 1; 

        // 将json对象序列化为字符串，并发送该消息
        conn->send(response.dump());
    } 
}

// 一对一聊天业务（聊天消息格式：{"msgid":5,"id":*,"from":"***","toid":*,"msg":"***"}）
void ChatService::oneChat(const TcpConnectionPtr& conn, const json& js, Timestamp time)
{
    int toid = js["toid"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto iter = _userConnMap.find(toid);  // _userConnMap存储在线用户的通信连接<userid, TcpConnectionPtr>
        if (iter != _userConnMap.end())
        {
            // toid在线，则转发消息（加锁保证了转发消息期间，_connMutex的线程安全，即该conn不会在转发消息过程中被清除掉）
            iter->second->send(js.dump());   // 服务器主动推送消息给toid用户
            /* 此时，表明通信双方均在服务器集群中的一台服务器上 */
            return;
        }   

    }

    // 查询toid是否在线，在线则表示通信双方不在同一个服务器集群上，需要通过redis消息中间件完成通信
    User user = _userModel.query(toid);
    if (user.getState() == "online")
    {
        _redis.publish(toid, js.dump());  // 发布数据到redis中以接收方用户userid命名的管道上
        return;
    }

    // toid不在线，则存储为离线消息
    _offlineMsgModel.insert(toid, js.dump());  // 将JSON对象转换为JSON字符串
}

// 添加好友业务
bool ChatService::addFriend(const TcpConnectionPtr& conn, const json& js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    if (!_friendModel.insert(userid, friendid))
    {
        LOG_INFO << "addFriend failure!";
        return false;
    }
    LOG_INFO << "addFriend success!";
    return true;
}

// 创建群组业务
bool ChatService::createGroup(const TcpConnectionPtr& conn, const json& js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1, name, desc);  // group的id字段在数据库Group表中是自增长列，故无序自己指定
    if (_groupModel.createGroup(group))  // 将group群组加入到数据库Group表中，会自动修改group的id成员的值（为数据库表中的自增长id字段的当前值）
    {
        // 创建群组成功后，添加创建人的信息
        if (_groupModel.addGroup(userid, group.getID(), "creator"))
        {
            LOG_INFO << "createGroup success!";
            return true;
        } 
    } 
    LOG_INFO << "createGroup failure!";   // 创建失败，只有服务器的运行日志会提示
    return false;
    /*
    json response;
    response["msgid"] = CREATE_GROUP_MSG_ACK;
    response["errmsg"] = "createGroup failure!!!"; 
    conn->send(response.dump());
    */ 
}

// 加入群组业务
bool ChatService::addGroup(const TcpConnectionPtr& conn, const json& js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    if (! _groupModel.addGroup(userid, groupid))  // 默认加入群组的用户角色为"normal"
    {
        LOG_INFO << "addGroup failure!";
        return false;
    }
    LOG_INFO << "addGroup success!";   
    return true;
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr& conn, const json& js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
 
    vector<int> groupUserIDs = _groupModel.queryGroupUsers(userid, groupid);

    lock_guard<mutex> lock(_connMutex);  // 保证_userConnMap的线程安全
    for (auto groupUserID : groupUserIDs)
    { 
        auto iter = _userConnMap.find(groupUserID);
        if (iter != _userConnMap.end())
        {
            // 当前groupUserID用户在线，则转发群组消息
            iter->second->send(js.dump());
        }
        else
        {
            User user = _userModel.query(groupUserID);
            if (user.getState() == "online")
            {
                // 查询toid是否在线，在线则表示通信双方不在同一个服务器集群上，需要通过redis消息中间件完成通信
                _redis.publish(groupUserID, js.dump());  // 发布数据到redis中以接收方用户userid命名的管道上
                return;
            }
            else
            {
                // 当前groupUserID用户不在线，则转存为离线群消息
                _offlineMsgModel.insert(groupUserID, js.dump());
            } 
        }
    }
}

// redis订阅消息触发的回调函数（这里channel其实就是userid）
void ChatService::handleRedisSubscribeMessage(int channel, string message)
{
    lock_guard<mutex> lock(_connMutex);
    auto iter = _userConnMap.find(channel);
    if (iter != _userConnMap.end())
    {
        iter->second->send(message);
        return;
    }

    /* 当订阅到消息（程序执行到该函数）后，突然用户下线了，则直接转存为离线消息 */
    // 存储该用户的离线消息
    _offlineMsgModel.insert(channel, message);
}