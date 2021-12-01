#include "sql_connection_pool.h"

connection_pool::connection_pool(){
    m_cur_conn = 0;
    m_free_conn = 0;
}

connection_pool::~connection_pool(){
    list<MYSQL*>::iterator iter;
    for(iter = conn_list.begin(); iter != conn_list.end(); ++iter){
        MYSQL *conn = *iter;
        mysql_close(conn);
    }
}

// 初始化连接池
void connection_pool::init(string url, string user, string password, string database_name, int port, int max_conn, int close_log){
    m_url = url;
    m_port = port;
    m_user = user;
    m_password = password;
    m_database_name = database_name;
    m_close_log = close_log;

    // 构造连接
    for(int i = 0; i < max_conn; ++i){
        // 初始化一个mysql连接的实例对象，MYSQL* mysql_init(MYSQL *mysql);
        MYSQL* conn = nullptr;
        conn = mysql_init(conn);
        if(conn == nullptr){
            LOG_ERROR("MySQL init Error");
            exit(1);
        }

        // 与数据库引擎建立连接
        conn = mysql_real_connect(conn, url.c_str(), user.c_str(), password.c_str(), database_name.c_str(), port, nullptr, 0);
        if(conn == nullptr){
            LOG_ERROR("MySQL real connect Error");
            exit(1);
        }

        // 添加到连接链表
        conn_list.push_back(conn);
        ++m_free_conn;
    }

    // 创建信号量
    reserve = semaphore(m_free_conn);
    m_max_conn = m_free_conn;
}

// 有请求时，从数据库连接池返回一个可用连接
MYSQL* connection_pool::get_connection(){
    // 无空闲连接
    if(conn_list.size() == 0)
        return nullptr;
    
    MYSQL *conn = nullptr;
    // 等待空闲连接
    reserve.wait();
    // 加互斥锁
    lock.lock();
    // 取连接池第一个连接
    conn = conn_list.front();
    conn_list.pop_front();

    --m_free_conn;
    ++m_cur_conn;

    lock.unlock();
    return conn;
}

// 释放当前使用的连接，成功返回true
bool connection_pool::release_connection(MYSQL *conn){
    if(conn == nullptr)
        return false;

    lock.lock();
    conn_list.push_back(conn);
    ++m_free_conn;
    --m_cur_conn;
    lock.unlock();

    reserve.post();
    return true;
}

// 当前空闲连接数
int connection_pool::get_freeconn(){
    return this->m_free_conn;
}

// 从连接池获取一个数据库连接
connectionRAII::connectionRAII(MYSQL **conn, connection_pool *connPool){
    *conn = connPool->get_connection();
    connRAII = *conn;
    poolRAII = connPool;
}

// 释放持有的数据库连接
connectionRAII::~connectionRAII(){
    poolRAII->release_connection(connRAII);
}