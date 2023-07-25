#include "redis.h"
#include <iostream>
using namespace std;

Redis::Redis() : _publish_context(nullptr), _subcribe_context(nullptr)
{ }

Redis::~Redis()
{
    if (_publish_context != nullptr)
    {
        // 断开 Redis-Server 的连接（关闭 Socket）并释放 _publish_context 对象所占用的内存。
        redisFree(_publish_context);
    }

    if (_subcribe_context != nullptr)
    {
        // 断开 Redis-Server 的连接（关闭 Socket）并释放 _subcribe_context 对象所占用的内存。
        redisFree(_subcribe_context);
    }
}

// 初始化向业务层上报通道消息的回调对象
void Redis::init_notify_handler(function<void(int,string)> fn)
{
    _notify_message_handler = fn;
}

// 连接redis服务器 
bool Redis::connect()
{
    const char ip[10] = "127.0.0.1";
    int port = 6379;
    const char* password = "sgy211810";

    // 负责publish发布消息的上下文连接
    _publish_context = redisConnect(ip, port); 
    if (_publish_context == nullptr || _publish_context->err)
    {
        cerr << "connect redis failed!" << endl;
        return false;
    }

    // 负责subscribe订阅消息的上下文连接
    _subcribe_context = redisConnect(ip, port);
    if (_subcribe_context == nullptr || _subcribe_context->err)
    {
        cerr << "connect redis failed!" << endl;
        return false;
    }

    /*
        排查好久出问题的原因：登录redis时没有传入密码，故没有通过身份验证，subscribe时会报错。
        ErrType:6, ErrInfo:NOAUTH Authentication required
    */
    // 发送AUTH命令进行身份验证，_publish_context、_subcribe_context属于不同的进程上下文故需要分别验证身份
    auto reply = static_cast<redisReply*>(redisCommand(_publish_context, "AUTH %s", password));
    if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) 
    {
        cerr << "_publish_context AUTH failure: " << reply->str << endl;
        return false;
    }
    reply = static_cast<redisReply*>(redisCommand(_subcribe_context, "AUTH %s", password));
    if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) 
    {
        cerr << "_subcribe_context AUTH failure: " << reply->str << endl;
        return false;
    }
    freeReplyObject(reply);


    // 在单独的线程中，监听通道上的事件，有消息给业务层进行上报
    thread t([&]() {
        observer_channel_message();
    });
    t.detach();   // 设置线程未“分离状态”，即将t线程变成单独的线程

    cout << "connect redis-server success!" << endl;

    return true;
}

// 向redis指定的通道channel发布消息
bool Redis::publish(int channel, string message)
{
    redisReply *reply = (redisReply *)redisCommand(_publish_context, "PUBLISH %d %s", channel, message.c_str());
    if (reply == nullptr)
    {
        cerr << "publish command failed!" << endl;
        return false;
    }

    // 释放 redisReply 指针变量所占用的内存（会递归的释放数组中的资源）
    freeReplyObject(reply);

    return true;
}

/*
    由于线程池里面的redisGetReply抢了上面订阅subscribe的redisCommand底层调用的redisGetReply的响应消息，导致ChatServer线程阻塞在这个接口调用上，无法再次回到epoll_wait处了，这个线程就废掉了，如果工作线程全部发生这种情况，最终服务器所有的工作线程就全部停止工作了！
    本项目中，redis.cpp中subscribe(int channel)和unsubscribe(int channel)都涉及到这种问题。
    解决方案：
    从hiredis的redisCommand源码上可以看出，它实际上相当于调用了这三个函数：
        1）redisAppendCommand 把命令写入本地发送缓冲区
        2）redisBufferWrite 把本地缓冲区的命令通过网络发送出去
        3）redisGetReply 阻塞等待redis server响应消息
    既然在muduo库的ThreadPool中单独开辟了一个线程池，接收this->_context上下文的响应消息，因此subcribe订阅消息只做消息发送，不做消息接收就可以了。
    详细的问题和分析：https://blog.csdn.net/QIANGWEIYUAN/article/details/97895611。
*/
// 向redis指定的通道subscribe订阅消息
bool Redis::subscribe(int channel)
{ 
    // SUBSCRIBE命令本身会造成线程阻塞等待通道里面发生消息，这里只做订阅通道，不接收通道消息
    // 通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    // 只负责发送命令，不阻塞接收redis-server响应消息，否则会和notifyMsg线程抢占响应资源
    if (redisAppendCommand(this->_subcribe_context, "SUBSCRIBE %d", channel) == REDIS_ERR)
    {
        cerr << "subscribe [" << channel << "] failure!" << endl;
        return false;
    }
 
    // redisBufferWrite可以“循环发送”缓冲区，直到缓冲区数据发送完毕后，done被置为1
    int done = 0;
    while (done != 1)
    {
        if (redisBufferWrite(_subcribe_context, &done) == REDIS_ERR)
        {
            cerr << "subscribe [" << channel << "] failure!" << endl;
            return false;
        } 
    } 

    cout << "subscribe [" << channel << "] success!" << endl;
    return true;
}
// 向redis指定的通道unsubscribe取消订阅消息
bool Redis::unsubscribe(int channel)
{
    if (redisAppendCommand(this->_subcribe_context, "UNSUBSCRIBE %d", channel) == REDIS_ERR)
    {
        cerr << "unsubscribe [" << channel << "] failure!" << endl;
        return false;
    }

    // redisBufferWrite可以“循环发送”缓冲区，直到缓冲区数据发送完毕后，done被置为1
    int done = 0;
    while (!done)
    {
        if (redisBufferWrite(this->_subcribe_context, &done) == REDIS_ERR)
        {
            cerr << "unsubscribe [" << channel << "] failure!" << endl;
            return false;
        }
    }
    cout << "unsubscribe [" << channel << "] success!" << endl;
    return true;
}

// 在独立线程中，接收订阅通道中的消息
void Redis::observer_channel_message()
{
    redisReply *reply = nullptr;
    while (redisGetReply(this->_subcribe_context, (void**)&reply) == REDIS_OK)
    { 
        /*
            订阅收到的消息是一个带三元素的数组：
                { subscribe、订阅成功的频道名称、当前客户端订阅的频道数量 }
 重点关注的对象：{ message、产生消息的频道名称、消息的内容 }
                { unsubscribe、对应的频道名称、当前客户端订阅的频道数量 }
        */   
        if (reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {
            cout << ".................. " << reply->element[1]->str << ", " << reply->element[2]->str << " ................" << endl;
            // 给业务层上报通道上发生的消息
            _notify_message_handler(atoi(reply->element[1]->str) , reply->element[2]->str);
        }

        freeReplyObject(reply);  // 释放 redisReply 指针变量所占用的内存（会递归的释放数组中的资源）
    }

    cerr << "------------------- oberver_channel_message quit -------------------" << endl;
}
