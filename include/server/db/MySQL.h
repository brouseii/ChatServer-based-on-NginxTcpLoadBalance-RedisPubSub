#ifndef MYSQL_H
#define MYSQL_H

#include <mysql/mysql.h>
#include <string>
#include <ctime>
using namespace std;

/* 实现MySQL数据库的操作 */
class MySQL
{
public:
	// 初始化数据库连接
	MySQL(string ip = "127.0.0.1", string username = "root", string password = "sgy211810", string dbname = "chat");
	// 释放数据库连接资源
	~MySQL();

	// 连接数据库
	bool connect();

	// 更新操作 insert、delete、update
	bool update(string sql);
	
	// 查询操作 select
	MYSQL_RES* query(string sql); 
	void PrintQueryResult(MYSQL_RES* results); 

	MYSQL* getConnection() { return _conn; }
private:
	MYSQL* _conn;        // 与MySQL Server的一条连接 

	string _ip;
	string _username;
	string _password;
	string _dbname;
};

#endif