#ifndef USER_H
#define USER_H

#include <string>
using namespace std;

// User表的ORM类（Object Relation Map对象关系映射）
class User
{
public:
    User(int id = -1, string name = "", string pwd = "", string state = "offline")
        : _id(id), _name(name), _pwd(pwd), _state(state)  
    { }

    void setID(int id) { _id = id; }
    void setName(string name) { _name = name; }
    void setPwd(string pwd) { _pwd = pwd; }
    void setState(string state) { _state = state; }

    int getID() { return _id; }
    string getName() { return _name; }
    string getPwd() { return _pwd; }
    string getState() { return _state; }
protected:
    int _id;
    string _name;
    string _pwd;
    string _state;
};



#endif