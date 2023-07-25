#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "Group.hpp"
#include <string>
#include <vector>
using namespace std;

// 维护群组信息的操作接口方法
class GroupModel
{
public:
    // 创建群组
    bool createGroup(Group& group);
    
    // 加入群组
    bool addGroup(int userid, int groupid, string role="normal");

    // 查询用户所在群组信息
    vector<Group> queryGroups(int userid);

    // 根据指定的groupid查询，除userid自己外，群组用户id列表（主要用于群聊业务给群组其他成员群发消息）
    vector<int> queryGroupUsers(int userid, int groupid); 
};

#endif