#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "../threadpool/threadpool.h"
#include "../http/http_conn.h"

const int MAX_FD = 65535;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;             //最小超时单位

class WebServer{
public:
    WebServer(string user, string password, string database_name, char *root,
              int port, int close_log, int async_log, int sql_num,
              int thread_num, int actor_model, int trig_mode, int opt_linger);
    ~WebServer();

    void log_write();
    void sql_pool();
    void thread_pool();
    
    void eventListen();
    void eventLoop();

    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(util_timer *timer);
    void deal_timer(util_timer *timer, int sockfd);
    bool dealclinetdata();
    bool dealwithsignal(bool& timeout, bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

public:
    // 基本配置
    string m_user;          // 登陆数据库用户名
    string m_passWord;      // 登陆数据库密码
    string m_databaseName;  // 数据库名
    char *m_root;           // 资源路径
    int m_port;             // 端口号
    int m_close_log;        // 关闭日志，0不关闭，1关闭
    int m_async_log;        // 日志写入方式，0同步，1异步
    int m_sql_num;          // 数据库池连接数
    int m_thread_num;       // 线程池线程数
    int m_actormodel;       // 并发模型，0 proactor，1 reactor
    int m_listen_trig_mode; // 监听触发模式，0 LT，1 ET
    int m_conn_trig_mode;   // 连接触发模式，0 LT，1 ET
    int m_opt_linger;       // 优雅关闭链接，0不使用，1使用

    connection_pool *m_connPool;    // 数据库连接池
    threadpool<http_conn> *m_pool;  // 线程池

    http_conn *users;               // 请求结构
    client_data *users_timer;       // 定时器
    Utils utils;

    int m_listenfd;         // 监听套接字
    int m_pipefd[2];

    int m_epollfd;                          // epoll句柄
    epoll_event events[MAX_EVENT_NUMBER];   // 事件列表
};

#endif