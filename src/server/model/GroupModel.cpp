#include "GroupModel.h"
#include "MySQL.h"
#include "muduo/base/Logging.h" 

// 创建群组
bool GroupModel::createGroup(Group& group)
{
    // 组装sql语句：
    char sql[1024] = {0};
    sprintf(sql, "insert into AllGroup(groupname, groupdesc) values('%s', '%s')"
        , group.getName().c_str(), group.getDesc().c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            // mysql_insert_id()：获取最近插入的数据的自增id
            // MYSQL::getConnection()：获取MySQL数据库连接句柄
            group.setID(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

// 加入群组
bool GroupModel::addGroup(int userid, int groupid, string role)
{
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into GroupUser values(%d,%d,'%s')", groupid, userid, role.c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        if (!mysql.update(sql))
        {  
            LOG_INFO << sql << ", addGroup failure!";
            return false;
        }
    } 
    return true;
}

// 查询用户所在群组信息
vector<Group> GroupModel::queryGroups(int userid)
{
    /*
        内连接多表查询：降低访问mysql服务器的次数即降低网络IO、与底层存储引擎的IO次数。
        1.先根据userid在groupuser表中，查询出该用户所属的群组信息。
        2.再根据群组信息，查询属于该群组所有用户的userid，并且和user表进行多表联合查询，查出用户的详细信息。
    */
    char sql[1024] = {0};
    sprintf(sql
    , "select a.id,a.groupname,a.groupdesc from AllGroup a inner join GroupUser b on a.id=b.groupid where b.userid=%d"
    , userid);

    vector<Group> groups;  // Group <==> { int _id、string _name、string _desc、vector<GroupUser> _users }

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
            // 1、查出userid所有的群组信息
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                Group group;
                group.setID(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                groups.push_back(group);
            }  
        }

        // 2、查询每个群组的用户信息
        for (Group& group : groups)
        {
            sprintf(sql
            , "select a.id,a.name,a.state,b.grouprole from User a inner join GroupUser b on a.id = b.userid where b.groupid=%d"
            , group.getID());

            res = mysql.query(sql);
            if (res != nullptr)
            {
                MYSQL_ROW row; 
                while ((row = mysql_fetch_row(res)) != nullptr)
                {
                    GroupUser groupUser;
                    groupUser.setID(atoi(row[0]));
                    groupUser.setName(row[1]);
                    groupUser.setState(row[2]);
                    groupUser.setRole(row[3]);
                    group.getUsers().push_back(groupUser);
                }  
            }
            // 释放res的资源
            mysql_free_result(res);  
        }
    }

    return groups;
}

// 根据指定的groupid查询，除userid自己外，群组用户id列表（主要用于群聊业务给群组其他成员群发消息）
vector<int> GroupModel::queryGroupUsers(int userid, int groupid)
{
    char sql[1024] = {0};
    sprintf(sql, "select userid from GroupUser where groupid=%d and userid!=%d", groupid, userid);

    vector<int> otherUserIDs;

    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                otherUserIDs.push_back(atoi(row[0]));
            } 
        }
        mysql_free_result(res);
    }

    return otherUserIDs;
}