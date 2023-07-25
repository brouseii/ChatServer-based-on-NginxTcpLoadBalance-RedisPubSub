#ifndef ALLGROUP_H
#define ALLGROUP_H

#include <string>
#include <vector>
#include "GroupUser.h"
using namespace std;

// User表的ORM类
class Group
{
public:
    Group(int id = -1, string name = "", string desc = "")
        : _id(id), _name(name), _desc(desc)
    { }

    void setID(int id) { this->_id = id; }
    void setName(string name) { this->_name = name; }
    void setDesc(string desc) { this->_desc = desc; }

    int getID() { return this->_id; }
    string getName() { return this->_name; }
    string getDesc() { return this->_desc; }
    vector<GroupUser>& getUsers() { return this->_users; }
private:
    int _id;
    string _name;
    string _desc;
    vector<GroupUser> _users;
};

#endif