#include "OfflineMsgModel.h"
#include "MySQL.h"
#include <muduo/base/Logging.h>

// 存储用户的离线消息
void OfflineMsgModel::insert(int userid, string msg)
{ 
    // 转义JSON字符串，以避免在插入到数据库中时出现语法错误。
    char* msg_escaped = new char[(msg.length() * 2) + 1];;
    mysql_escape_string(msg_escaped, msg.c_str(), msg.length()); 
    // 组装sql语句，插入转义后的JSON字符串
    string sql = "insert into OfflineMessage values(" + to_string(userid) + ",'" + string(msg_escaped) + "')";
    delete msg_escaped;

    MySQL mysql;  
    if (mysql.connect())
    {
        if (!mysql.update(sql))
        {
            LOG_INFO << "insert OfflineMessage failure!";
            exit(-1);
        }
    }
}

// 删除用户的离线消息
void OfflineMsgModel::remove(int userid)
{
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "delete from OfflineMessage where userid=%d", userid);
    
    MySQL mysql;
    if (mysql.connect())
    {
        if (!mysql.update(sql))
        {
            LOG_INFO << "remove OfflineMessage failure!";
            exit(-1);
        }
    }
}

// 查询用户的离线消息
vector<string> OfflineMsgModel::query(int userid)
{
    // sql查询语句：
    char sql[1024] = {0};
    sprintf(sql, "select message from OfflineMessage where userid=%d", userid);
    
    vector<string> messages;

    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if (res != nullptr)
        {  
            /*
                MYSQL_ROW mysql_fetch_row(MYSQL_RES* result)
                Fetches the next row from the result set
            */
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                messages.push_back(string(row[0])); 
            } 
        }
        // 释放res的资源
        mysql_free_result(res);
    }  

    return messages;
}