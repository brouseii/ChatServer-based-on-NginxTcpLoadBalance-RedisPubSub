#ifndef USERMODEL_H
#define USERMODEL_H

#include "User.hpp"

// User表的数据操作类
class UserModel
{
public:
    // 给User表插入元素：
    bool insert(User& user);
    
    // 在User表中，查找元素：
    User query(int id);

    // 更新User表中，用户的状态信息：
    bool updateState(User& user);

    // 重置用户的状态信息
    void resetState();
};

#endif