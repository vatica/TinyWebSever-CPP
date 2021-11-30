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
    MYSQL* GetConnetion();                  // 获取数据库连接
    bool ReleaseConnection(MYSQL *conn);    // 释放连接
    int GetFreeConn();                      // 获取连接
    void DestroyPool();                     // 销毁数据库连接池

    // 单例模式，静态方法
    static connection_pool* GetInstance();
    // 初始化数据库连接池
    void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log);

private:
    // 私有化构造和析构函数，提供构造和析构方法，实现单例模式
    connection_pool();
    ~connection_pool();

    int m_MaxConn;  // 最大连接数
    int m_CurConn;  // 当前已使用连接数
    int m_FreeConn; // 当前空闲连接数
    mutexlocker lock;       // 互斥锁
    list<MYSQL*> connList;  // 连接池
    semaphore reserve;      // 信号量，指示是否有空闲连接

public:
    string m_url;  // 主机地址
    string m_Port; // 数据库端口号
    string m_User; // 数据库用户名
    string m_PassWord;      // 数据库密码
    string m_DatabaseName;  // 数据库名
    int m_close_log;        // 日志开关
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