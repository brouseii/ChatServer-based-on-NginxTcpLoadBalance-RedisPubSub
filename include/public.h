#ifndef PUBLIC_H
#define PUBLIC_H

/* server和client的公共文件 */
enum EnMsgType
{
    LOGIN_MSG = 1,     // 登录消息  {"msgid":1,"id":*,"password":"***"}
    LOGIN_MSG_ACK,     // 登录响应消息
 
    LOGINOUT_MSG,      // 注销消息

    SIGNUP_MSG,        // 注册消息  {"msgid":3,"id":*,"name":"***","password":*}
    SIGNUP_MSG_ACK,    // 注册响应消息

    ONE_CHAT_MSG,      // 聊天消息  {"msgid":5,"id":*,"from":"***","to":*,"msg":"***"}

    ADD_FRIEND_MSG,    // 添加好友消息

    CREATE_GROUP_MSG,          // 创建群组   CREATE_GROUP_MSG_ACK,  // 创建群组响应消息 
    ADD_GROUP_MSG,             // 加入群组
    GROUP_CHAT_MSG,            // 群聊天
};

#endif
