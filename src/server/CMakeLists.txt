# 定义一个SRC_LIST变量，包含了该目录下所有的源文件
aux_source_directory(. SRC_LIST)
aux_source_directory(./db DB_LIST)
aux_source_directory(./model MODEL_LIST)
aux_source_directory(./redis REDIS_LIST)

# 指定生成可执行文件
add_executable(ChatServer ${SRC_LIST} ${DB_LIST} ${MODEL_LIST} ${REDIS_LIST})

# 指定生成可执行文件链接时，需要依赖的库文件
target_link_libraries(ChatServer muduo_net muduo_base mysqlclient hiredis pthread)

# 链接时，遇到的问题：
# 1. 目录/usr/include/mysql不存在，则需要安装sudo rpm -ivh mysql-community-devel-5.7.28-1.el7.x86_64.rpm
# 2. cannnot find -lmysqlclient，则需要将libmysqlclient.so放入/usr/lib/文件夹下
