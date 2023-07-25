#include "MySQL.h"
#include <muduo/base/Logging.h> 

MySQL::MySQL(string ip, string username, string password, string dbname)
	: _ip(ip), _username(username), _password(password), _dbname(dbname)
{
	/* 
		初始化数据库连接：
		MYSQL* mysql_init(MYSQL* mysql);
	*/
	_conn = mysql_init(nullptr);
}

MySQL::~MySQL()
{
	/*
		释放数据库连接资源：
		void mysql_close(MYSQL* sock);
	*/
	if (_conn != nullptr)
	{
		mysql_close(_conn);
	}
}

bool MySQL::connect()
{
	/*
		连接数据库：
		MYSQL* mysql_real_connect(MYSQL *mysql, const char *host, const char *user, const char *passwd, const char *db, unsigned int port
	                          , const char* unix_socket, unsigned long clientflag);
	*/
	MYSQL* p = mysql_real_connect(_conn, _ip.c_str(), _username.c_str(), _password.c_str(), _dbname.c_str(), 3306, nullptr, 0);
	
	if (p != nullptr)
	{
		// C/C++代码默认的编码字符是ASCII码，为避免中文显示乱码，采用gbk编码格式
		mysql_query(_conn, "set name gbk");
		LOG_INFO << "connect mysql success!";
	}
	else
	{
		LOG_INFO << "connect mysql fail!";
	}
	
	return p != nullptr;
}

/*
	更新mysql的操作：
	int mysql_query(MYSQL* mysql, const char* sql);
	Return Values Zero for success, Nonzero if an error occurred.
*/
bool MySQL::update(string sql)
{
	// 更新操作：insert/delete/update  
	if (mysql_query(_conn, sql.c_str()))
	{
		LOG_INFO << __FILE__ << " : " << __LINE__ << " : " << sql << ", insert/delete/update failure!";
		return false;
	}
	return true;
}

MYSQL_RES* MySQL::query(string sql)
{
	// 查询操作：select
	if (mysql_query(_conn, sql.c_str()))
	{
		LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                 << sql << ", query failure!";
		return nullptr;
	}

	/*
		Retrieves a complete result set to the client, allocates a MYSQL_RES structure, and places the result into this structure.
		1、returns a null pointer if the statement did not return a result set.
		   for example, if it was an INSERT statement or if reading of the result set failed.
		2、You can check whether an error occurred by checking whether mysql_error() returns a nonempty string.
		   Return Values:A MYSQL_RES result structure with the results.NULL(0) if an error occurred.
	*/ 
	return mysql_store_result(_conn);
} 

void MySQL::PrintQueryResult(MYSQL_RES* results)
{
	/*
		my_ulonglong mysql_affected_rows(MYSQL* mysql)
		1、It returns the number of rows changed, deleted, or inserted by the last statement if it was an UPDATE, DELETE, or INSERT.
		2、For SELECT statements, returns the number of rows in the result set.
	*/ 
	LOG_INFO << "Number of dataline returned: " << mysql_affected_rows(_conn) << "\n";

	// 获取列数：
	int j = mysql_num_fields(results);

	// 获取字段名：
	/*
		MYSQL_FIELD* mysql_fetch_field(MYSQL_RES* result)
		1、Returns the definition of one column of a result set as a MYSQL_FIELD structure.
		2、Call this function repeatedly to retrieve information about all columns in the result set.
	*/
	char* str_field[32];
	for (int i = 0; i < j; ++i)
	{
		str_field[i] = mysql_fetch_field(results)->name;
		printf("%10s\t", str_field[i]);
	}
	printf("\n");

	// 打印查询结果：
	/*
		MYSQL_ROW mysql_fetch_row(MYSQL_RES* result)
		Fetches the next row from the result set
	*/
	MYSQL_ROW column;
	while (column = mysql_fetch_row(results))
	{
		for (int i = 0; i < j; ++i)
		{
			printf("%10s\t", column[i]);
		}
		printf("\n");
	} 
} 