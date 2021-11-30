#include <mysql/mysql.h>
#include <cstdio>
#include <string>
#include <cstring>
#include <cstdlib>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool(){
    m_CurConn = 0;
    m_FreeConn = 0;
}

// RAII机制销毁连接池
connection_pool::~connection_pool(){
    DestroyPool();
}

// 懒汉式单例模式，静态变量只会初始化一次
connection_pool* connection_pool::GetInstance(){
    static connection_pool connPool;
    return &connPool;
}

// 初始化连接池
void connection_pool::init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log){
    m_url = url;
    m_Port = Port;
    m_User = User;
    m_PassWord = PassWord;
    m_DatabaseName = DataBaseName;
    m_close_log = close_log;

    // 构造连接
    for(int i = 0; i < MaxConn; ++i){
        // 初始化一个mysql连接的实例对象，MYSQL* mysql_init(MYSQL *mysql);
        MYSQL* conn = nullptr;
        conn = mysql_init(conn);
        if(conn == nullptr){
            LOG_ERROR("MySQL init Error");
            exit(1);
        }

        // 与数据库引擎建立连接
        // MYSQL* mysql_real_connect(MYSQL *mysql, const char *host, const char *user, const char *passwd, const char *db, 
        //                           unsigned int port, const char *unix_socket, unsigned long client_flag)
        conn = mysql_real_connect(conn, url.c_str(), User.c_str(), PassWord.c_str(), DataBaseName.c_str(), Port, nullptr, 0);
        if(conn == nullptr){
            LOG_ERROR("MySQL real connect Error");
            exit(1);
        }

        connList.push_back(conn);
        ++m_FreeConn;
    }

    // 创建条件变量
    reserve = semaphore(m_FreeConn);
    m_MaxConn = m_FreeConn;
}

// 有请求时，从数据库连接池返回一个可用连接
MYSQL* connection_pool::GetConnetion(){ 
    if(connList.size() == 0)
        return nullptr;
    
    MYSQL *conn = nullptr;
    // 等待空闲连接
    reserve.wait();
    // 加互斥锁
    lock.lock();
    // 取连接池第一个连接，临界区
    conn = connList.front();
    connList.pop_front();

    --m_FreeConn;
    ++m_CurConn;

    lock.unlock();
    return conn;
}

// 释放当前使用的连接，成功返回true
bool connection_pool::ReleaseConnection(MYSQL *conn){
    if(conn == nullptr)
        return false;

    // 进入临界区
    lock.lock();
    connList.push_back(conn);
    ++m_FreeConn;
    --m_CurConn;
    lock.unlock();

    reserve.post();
    return true;
}

// 销毁数据库连接池
void connection_pool::DestroyPool(){
    lock.lock();
    // 遍历链表，销毁mysql连接
    if(connList.size() > 0){
        list<MYSQL*>::iterator iter;
        for(iter = connList.begin(); iter != connList.end(); ++iter){
            MYSQL *conn = *iter;
            mysql_close(conn);
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();
    }
    lock.unlock();
}

// 当前空闲连接数
int connection_pool::GetFreeConn(){
    return this->m_FreeConn;
}


// 从连接池获取一个数据库连接
connectionRAII::connectionRAII(MYSQL **conn, connection_pool *connPool){
    *conn = connPool->GetConnetion();
    connRAII = *conn;
    poolRAII = connPool;
}

// 释放持有的数据库连接
connectionRAII::~connectionRAII(){
    poolRAII->ReleaseConnection(connRAII);
}