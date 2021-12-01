#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include <cstdio>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <cstring>
#include <iostream>
#include <string>
#include "../locker/locker.h"
#include "../log/log.h"

using namespace std;

// 数据库连接池
class connection_pool{
public:
    ~connection_pool(); 

    // 单例模式
    static connection_pool* get_instance(){
        static connection_pool connPool;
        return &connPool;
    }
    
    void init(string url, string user, string password, string database_name, int port, int max_conn, int close_log);
    MYSQL* get_connection();
    bool release_connection(MYSQL *conn);
    int get_freeconn();

private:
    connection_pool();

    string m_url;           // 主机地址
    string m_port;          // 数据库端口号
    string m_user;          // 数据库用户名
    string m_password;      // 数据库密码
    string m_database_name;  // 数据库名
    int m_close_log;        // 日志开关

    int m_max_conn;          // 最大连接数
    int m_cur_conn;          // 当前已使用连接数
    int m_free_conn;         // 当前空闲连接数
    mutexlocker lock;       // 互斥锁
    list<MYSQL*> conn_list;  // 连接池
    semaphore reserve;      // 信号量，指示是否有空闲连接   
};

// 资源获取即初始化
class connectionRAII{
public:
    connectionRAII(MYSQL **conn, connection_pool *connPool);
    ~connectionRAII();

private:
    MYSQL *connRAII;
    connection_pool *poolRAII;
};

#endif