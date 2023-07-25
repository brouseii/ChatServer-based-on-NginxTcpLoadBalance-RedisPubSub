#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>   // 提供了一组用于时间测量和处理的工具
#include <ctime>    // 提供了一组用于时间和日期处理的函数和类型
using namespace std;

#include "json.hpp"
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>   // 用于定义互联网协议中的数据结构和常量struct in_addr/sockaddr_in
#include <arpa/inet.h>    // 用于将网络字节序和主机字节序之间进行转换
#include <semaphore.h>
#include <atomic>

#include "Group.hpp"
#include "User.hpp"
#include "public.h"

#define NAME_LEN 50
#define PASSWORD_LEN 50
#define BUF_SIZE 1024

/* 直接在程序中保存了当前登录用户的信息，不用再访问数据库获取！ */
// 记录当前系统登陆的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;
// 显示当前登陆成功的用户基本信息
void showCurrentUserData();

/* 主线程mainThread作为向服务端发送请求消息的线程，子线程作为从服务端接收响应消息的线程 */
/* 接收线程 */
void readTaskHandler(int clientfd);
// 获取系统事件（聊天消息需要添加时间信息）
string getCurrentTime();
// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "Displays all supported commands, format \"help\""},
    {"chat", "One-on-one chat, format \"chat:friendid:message\""},
    {"addfriend", "add friend, format \"addfriend:friendid\""},
    {"creategroup", "create a group, format \"creategroup:groupname:groupdesc\""},
    {"addgroup", "add group, format \"addgroup:groupid\""},
    {"groupchat", "group chat, format \"groupchat:groupid:message\""},
    {"loginout", "logout, format \"loginout\""}
};
// "help" command handler
void help(int, string);
// "addfriend" command handler
void addfriend(int clientfd, string str);
// "chat" command handler
void chat(int clientfd, string str);
// "creategroup" command handler  groupname:groupdesc
void creategroup(int clientfd, string str);
// "addgroup" command handler
void addgroup(int clientfd, string str);
// "groupchat" command handler   groupid:message
void groupchat(int clientfd, string str);
// "loginout" command handler
void loginout(int clientfd, string);
// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}
};
// 记录客户端主聊天页面的运行状态
bool isMainMenuRunning = false;
/* 主聊天界面程序 */
void mainMenu(int clientfd);


// 信号量：用于读写线程之间进行通信
sem_t rwsem;
// 原子类型，用于保证多线程间的安全
atomic_bool g_isLoginSuccess(false);

//////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
    if (argc < 3) 
    {
        cerr << "command invaliad! Please input according to [./ChatClient 127.0.0.1 8000]." << endl;
        exit(-1);
    }

    // 通过解析命令行参数，获取客户端需要连接的服务端的ip和port
    char* ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        cerr << "socket create failure!" << endl;
        exit(-1);
    }

    // 将client提供的要连接的server信息ip+port，封装到sockaddr_in中
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);  // inet_addr()函数将IP地址转换为struct in_addr的对象

    // 将client和server进行tcp连接
    if (connect(clientfd, (sockaddr*)&server, sizeof(sockaddr_in)) == -1)
    {
        cerr << "connect server failure!" << endl;
        close(clientfd);
        exit(-1); 
    }

    // 初始化读写线程使用的信号量rwsem
    /*
    sem_init是一个用于初始化一个信号量（Semaphore）的函数，其函数原型如下：
        // sem是一个指向要初始化的信号量的指针，pshared指定信号量的共享方式，value指定信号量的初值
        int sem_init(sem_t *sem, int pshared, unsigned int value);
        // 返回值：0表示成功，-1表示失败。
    pshared参数指定信号量的共享方式，它可以取两个值：
        当pshared == 0时，表示信号量只能由“同一进程内的线程共享”。
        当pshared != 0时，表示信号量可以由“多个进程共享”，此时需要将sem指向的信号量放在共享内存中。
    value参数指定信号量的初值，即表示可以同时访问某个资源的线程或进程的数量。
        例如，如果value为1，则表示只允许一个线程或进程访问该资源，如果value为2，则表示允许两个线程或进程同时访问该资源。
    使用sem_init函数初始化一个信号量后，可以使用sem_wait和sem_post函数来对其进行加锁和解锁操作。
    */
    sem_init(&rwsem, 0, 0);
    /* 连接服务端成功后，开启接收子线程：负责接收服务端的应答数据 */
    thread readTask(readTaskHandler, clientfd);           // 底层调用的是pthread_create
    readTask.detach();   // 设置readTask线程为“分离状态”   // 底层调用的是pthread_deatch

    // client和server连接成功后，main线程开始“接收用户输入”和“发送数据”的逻辑
    for (;;)
    {
        // 显示首面菜单：登录、注册、退出
        cout << "============================" << endl;
        cout << "========= 1. login =========" << endl;
        cout << "========= 2. signup ========" << endl;
        cout << "========= 3. loginout ======" << endl;
        cout << "============================" << endl;

        cout << "please input your choice:";
        int choice = 0; 
        cin >> choice; cin.get(); // 读取缓冲区残留的回车

        switch (choice)
        {
            case 1:     // login
            {            
                int id = 0;
                char password[PASSWORD_LEN] = {0};
                cout << "userid:"; cin >> id; cin.get();  // 读掉缓冲区残留的回车`\n`
                cout << "password:"; cin.getline(password, PASSWORD_LEN);

                json js;
                js["msgid"] = LOGIN_MSG;
                js["id"] = id;
                js["password"] = password; 
                string request = js.dump();   // 将json对象序列化为字符串
    
                g_isLoginSuccess = false;

                // 将request通过clientfd发送给服务端
                // Send N bytes of BUF to socket FD. Returns the number sent or -1. 
                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if (len == -1) 
                {
                    cerr << "send login message failure! ==> " << request << endl; 
                } 
                
                // 阻塞等待信号量，子线程处理完服务端的登录应答消息后，会返回通知
                sem_wait(&rwsem); 

                if (g_isLoginSuccess) 
                {
                    isMainMenuRunning = true;
                    /* 进入聊天主菜单页面 */
                    mainMenu(clientfd); 
                } 

                break;
            }
            case 2:     // signup
            {
                char name[NAME_LEN] = {0};
                char password[PASSWORD_LEN] = {0};
                cout << "username:"; cin.getline(name, NAME_LEN);  // getline(__s,__n,widen('\n'))
                cout << "password:"; cin.getline(password, PASSWORD_LEN);

                json js;
                js["msgid"] = SIGNUP_MSG;
                js["name"] = name;
                js["password"] = password;
                string request = js.dump();
                
                // Send N bytes of BUF to socket FD. Returns the number sent or -1. 
                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if (len == -1) 
                {
                    cerr << "send signup message failure! ==> " << request << endl; 
                } 
                
                // 阻塞等待信号量，子线程处理完服务端的注册应答消息后，会返回通知
                sem_wait(&rwsem); 

                break;
            }
            case 3:     // quit 
            {
                close(clientfd);
                sem_destroy(&rwsem); // 用于同一进程的两个读写线程间通信的信号量，资源的释放
                exit(0); 
            }   
            default: 
            {
                cerr << "invalid input! Please input again." << endl;
                break;
            }
        }
    }

    return 0;
}

void doSignupResponse(json& responsejs)
{
    if (responsejs["errno"].get<int>() != 0)      // signup注册失败
    {   
        cerr << "name is already exist, register failure!" << endl;
    } 
    else                                          // signup注册成功
    {
        cout << "name has registered successfully, userid is " << responsejs["id"] 
            << ". Please remember its id." << endl;
    }
}

// 处理主线程登陆后，子线程收到服务端的应答消息后的业务逻辑
void doLoginResponse(json& responsejs)   
{ 
    if (responsejs["errno"].get<int>() != 0)  // login登录失败
    {   
        cerr << responsejs["errmsg"] << endl;
        g_isLoginSuccess = false;
    } 
    else                                      // login登录成功
    {                                    
        /* 1. 记录、显示当前系统登陆的用户信息 */
        // 记录当前用户
        g_currentUser.setID(responsejs["id"].get<int>());
        g_currentUser.setName(responsejs["name"]);

        // 记录当前用户的好友列表
        if (responsejs.contains("friends"))
        {
            vector<string> friendStrs = responsejs["friends"];
            for (const string& friendStr : friendStrs)
            {
                json friendjs = json::parse(friendStr); 
                g_currentUserFriendList.push_back(User(friendjs["id"].get<int>(), friendjs["name"], "", friendjs["state"]));
            }
        }

        // 记录当前用户的群组列表信息
        if (responsejs.contains("groups"))
        {
            vector<string> groupStrs = responsejs["groups"];
            for (string& groupStr : groupStrs)
            { 
                json groupjs = json::parse(groupStr);
                Group group(groupjs["id"].get<int>(), groupjs["groupname"], groupjs["groupdesc"]);
                
                vector<string> groupUserStrs = groupjs["users"];
                for (const string& groupUserStr : groupUserStrs)
                {
                    json groupUserjs = json::parse(groupUserStr);
                    GroupUser groupUser;
                    groupUser.setID(groupUserjs["id"].get<int>());
                    groupUser.setName(groupUserjs["name"]);
                    groupUser.setState(groupUserjs["state"]);
                    groupUser.setRole(groupUserjs["role"]);
                    group.getUsers().push_back(groupUser);
                }
                g_currentUserGroupList.push_back(group);
            }
        }

        // 显示登录用户的基本信息
        showCurrentUserData();

        cout << "........... offline message ..........." << endl;
        /* 2. 显示当前用户的离线消息，包括个人聊天信息、群组消息 */
        if (responsejs.contains("offlineMsg"))  
        {
            vector<string> offlineMsgs = responsejs["offlineMsg"];
            // ..........done...........
            for (string& offlineMsg : offlineMsgs)
            {
                json js = json::parse(offlineMsg);

                if (ONE_CHAT_MSG == js["msgid"].get<int>())
                {
                    // format ==> "time [id] : name said msg".
                    cout << js["time"].get<string>() << " [" << js["id"] << "] : " << js["name"].get<string>() << " said: " 
                        << js["msg"].get<string>() << endl;
                } 
                else
                {
                    // format ==> "[groupid]group's offline message: time [id]name said: msg".
                    cout << "[" << js["groupid"] << "]group's offline message: " 
                        << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>() 
                        << " said: " << js["msg"].get<string>() << endl;
                }
            }
        } 
        cout << "......................................." << endl; 

        g_isLoginSuccess = true;
    }
}

// 接收用户收到消息的子线程
void readTaskHandler(int clientfd)
{
    for (;;)
    {
        char buf[BUF_SIZE] = {0};
        /* 
            Read N bytes into BUF from socket FD.
            Returns the number read or -1 for errors.  
        */
        int len = recv(clientfd, buf, BUF_SIZE, 0);  // 阻塞等待服务端返回给clientfd的响应，并将其写入buf中 
        if (len == -1 || len == 0) 
        {
            close(clientfd);
            exit(-1);  
        } 
        // 调用json库中的json::parse()静态函数，把json字符串转换为json对象
        json js = json::parse(buf);   // 接收服务端ChatServer转发的数据，反序列化生成json数据对象
        int msgType = js["msgid"].get<int>();
        if (ONE_CHAT_MSG == msgType)
        {
            // format ==> "time [id]name said: msg".
            cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>() 
                << " said: " << js["msg"].get<string>() << endl;
            continue;
        }
        else if (GROUP_CHAT_MSG == msgType)
        {
            // format ==> "[groupid]group's offline message: time [id]name said: msg".
            cout << "[" << js["groupid"] << "]group's offline message: " 
                << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>() 
                << " said: " << js["msg"].get<string>() << endl;
            continue;
        }
        else if (LOGIN_MSG_ACK == msgType)
        {
            doLoginResponse(js);   // 处理主线程登陆后，子线程收到服务端的应答消息后的业务逻辑
            sem_post(&rwsem);      // 子线程处理完收到的服务端应答消息的业务逻辑后，通知主线程
            continue;
        } 
        else if (SIGNUP_MSG_ACK == msgType)
        {
            doSignupResponse(js);  // 处理主线程注册后，子线程收到服务端的应答消息后的业务逻辑
            sem_post(&rwsem);      // 子线程处理完收到的服务端应答消息的业务逻辑后，通知主线程
            continue;
        }
    }
}


// 主聊天界面程，clientfd根据输入的buf中的command调用相应的function
void mainMenu(int clientfd)
{
    help(-1, "");

    char buf[BUF_SIZE] = {0};
    while (isMainMenuRunning)
    {
        cin.getline(buf, BUF_SIZE);
        string commandbuf(buf);
        string command;   // 存储命令
        int idx = commandbuf.find(':');
        if (idx == -1) {
            command = commandbuf;   // commandMap中，help/logout的format格式决定的
        } else {
            command = commandbuf.substr(0, idx); // commandMap中，chat/addfriend/creategroup/addgroup/groupchat的format格式决定的
        }

        auto iter = commandHandlerMap.find(command);
        if (iter == commandHandlerMap.end())
        {
            cerr << "invalid input command!" << endl;
            continue;
        }
        // 调用相应命令的事件处理回调，mainMenu对修改封闭，添加新功能只需修改commandHandlerMap即可，不需要修改该函数
        iter->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx));   // 调用命令处理方法
    } 
} 

// "help" command handler <==> { commandformat <==> help }，其中help在str中已经被拿掉
void help(int, string)
{
    cout << "<<<<<<  show command list  >>>>>>" << endl;
    for (pair<const string,string>& cmdMap : commandMap) 
    {
        cout << cmdMap.first << " : " << cmdMap.second << endl;
    }
    cout << "<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>" << endl;
}
// "addfriend" command handler <==> { commandformat <==> addfriend:friendid }，其中addfriend在str中已经被拿掉
void addfriend(int clientfd, string str)
{ 
    int friendid = atoi(str.c_str());

    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getID();
    js["friendid"] = friendid;    
    string buf = js.dump();

    int len = send(clientfd, buf.c_str(), strlen(buf.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send addfriend message failure!" << endl;
    }
}
// "chat" command handler <==> { commandformat <==> chat:friendid:message }，其中chat在str中已经被拿掉
void chat(int clientfd, string str)
{
    int idx = str.find(':');
    if (idx == -1) 
    {
        cerr << "chat command invalid!" << endl;
        return;
    }

    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getID();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid; 
    js["msg"] = message; 
    js["time"] = getCurrentTime();
    string buf = js.dump();     // 将json对象序列化为字符串

    int len = send(clientfd, buf.c_str(), strlen(buf.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send chat message failure!" << endl;
    }
}
// "creategroup" command handler <==> { commandformat <==> creategroup:groupname:groupdesc }，其中creategroup在str中已经被拿掉
void creategroup(int clientfd, string str)
{    
    int idx = str.find(':');
    if (idx == -1) 
    {
        cerr << "creategroup command invalid!" << endl;
        return;
    }
   
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getID();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;  
    string buf = js.dump();     // 将json对象序列化为字符串

    int len = send(clientfd, buf.c_str(), strlen(buf.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send creategroup message failure!" << endl;
    }
}
// "addgroup" command handler <==> { commandformat <==> addgroup:groupid }，其中addgroup在str中已经被拿掉
void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());

    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getID();
    js["groupid"] = groupid;    
    string buf = js.dump();

    int len = send(clientfd, buf.c_str(), strlen(buf.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send addgroup message failure!" << endl;
    }
}
// "groupchat" command handler <==> { commandformat <==> groupchat:groupid:message }，其中groupchat在str中已经被拿掉
void groupchat(int clientfd, string str)
{
    int idx = str.find(':');
    if (idx == -1) 
    {
        cerr << "groupchat command invalid!" << endl;
        return;
    }

    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getID();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid; 
    js["msg"] = message; 
    js["time"] = getCurrentTime();
    string buf = js.dump();     // 将json对象序列化为字符串

    int len = send(clientfd, buf.c_str(), strlen(buf.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << buf << ", send groupchat message failure!" << endl;
    }
}
// "loginout" command handler <==> { commandformat <==> loginout }，其中loginout在str中已经被拿掉
void loginout(int clientfd, string str="")
{ 
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getID(); 
    string buf = js.dump();

    int len = send(clientfd, buf.c_str(), strlen(buf.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send loginout message failure!" << endl;
    }
    else 
    {
        // 客户端成功给服务端发送LOGINOUT_MSG后，退出主聊天界面
        isMainMenuRunning = false;
        
        // 初始化：防止同一个userid在loginout后重新登录，出现打印重复添加的问题
        g_currentUserFriendList.clear();
        g_currentUserGroupList.clear();
    }
}

// 获取系统事件（聊天消息需要添加时间信息）
string getCurrentTime()
{
    auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&time);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}

// 显示当前登陆成功的用户基本信息
void showCurrentUserData()
{ 
    cout << "================= login user information: =================" << endl;
    cout << "current login user :";
    cout << "userid=" << g_currentUser.getID() << ", username=" << g_currentUser.getName() << endl;
    cout << "................. login user's friend list: ................." << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User& user : g_currentUserFriendList)
        {
            cout << "userid=" << user.getID() << ", username=" << user.getName() << ", state=" << user.getState() << endl;
        }
    }
    cout << "................. login user's group list: ................." << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group& group : g_currentUserGroupList)
        {
            cout << "groupid=" << group.getID() << ", groupname=" << group.getName() << ", groupdesc=" << group.getDesc() << endl;
            for (GroupUser& user : group.getUsers())
            {
                cout << "userid=" << user.getID() << ", username=" << user.getName() << ", state=" << user.getState() << ", role=" << user.getRole() << endl;
            }
        }
    }                                        
    cout << "============================ end ============================" << endl;
}



