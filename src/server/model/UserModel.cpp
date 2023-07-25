#include "UserModel.h"
#include "MySQL.h"
#include <muduo/base/Logging.h>

using namespace std;

// 给User表插入元素：
bool UserModel::insert(User& user)
{
    // sql插入语句：
    char sql[1024] = {0};
    sprintf(sql, "insert into User(name, password, state) values('%s', '%s', '%s')"
        , user.getName().c_str()
        , user.getPwd().c_str()
        , user.getState().c_str());
    
    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            // 获取插入成功的用户数据生成的主键id
            user.setID(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
}

// 在User表中，查找元素：
User UserModel::query(int id)
{
    // sql查询语句：
    char sql[1024] = {0};
    sprintf(sql, "select * from User where id=%d", id);
    
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
            if (row = mysql_fetch_row(res))
            {
                User user;
                user.setID(atoi(row[0]));
                user.setName(row[1]);
                user.setPwd(row[2]);
                user.setState(row[3]); 

                // 释放res的资源
                mysql_free_result(res); 

                return user;
            }        
        }
    }  
    
    return User();
}

// 更新User表中，用户的状态信息：
bool UserModel::updateState(User& user)
{
    // sql更新语句：
    char sql[1024] = {0};
    sprintf(sql, "update User set state='%s' where id=%d", user.getState().c_str(), user.getID());

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

// 重置用户的状态信息
void UserModel::resetState()
{
    char sql[1024] = "update User set state='offline' where state='online'";

    MySQL mysql;
    if (mysql.connect())
    {
        if (!mysql.update(sql))
        {
            LOG_INFO << "resetState failure!";
            exit(-1);
        }
    }
}