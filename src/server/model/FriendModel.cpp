#include "FriendModel.h"
#include "MySQL.h"

// 添加好友关系
bool FriendModel::insert(int userid, int friendid)
{
    // sql插入语句：
    char sql[1024] = {0};
    sprintf(sql, "insert into Friend values(%d,%d)", userid, friendid);

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {  
            return true;
        }
    }
    return false;
}

// 返回用户好友列表
vector<User> FriendModel::query(int userid)
{
    // sql查询语句：
    char sql[1024] = {0}; 
    sprintf(sql, "select user.id,user.name,user.state from User user inner join Friend friend on friend.friendid=user.id where friend.userid=%d", userid);
    
    vector<User> users;

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
                // User::User(int id, string name, string pwd, string state)
                users.push_back(User(atoi(row[0]), row[1], "", row[2])); 
            } 
        }
        // 释放res的资源
        mysql_free_result(res);
    }  

    return users;
}