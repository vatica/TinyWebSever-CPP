#ifndef LST_TIMER_H
#define LST_TIMER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <time.h>
#include "../log/log.h"

// 前向声明
class util_timer;

// 客户端数据结构体
struct client_data{
    sockaddr_in address;    // 客户端地址
    int sockfd;             // 客户socket
    util_timer *timer;      // 定时器
};

// 定时器类
class util_timer{
public:
    util_timer():prev(nullptr), next(nullptr){}

public:
    time_t expire;                  // 超时时间
    void (*cb_func)(client_data*);  // 回调函数
    client_data *user_data;     // 连接资源
    util_timer *prev;           // 前指针
    util_timer *next;           // 后指针
};

// 定时器双向链表，按过期时间升序排序
class sort_timer_lst{
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer *timer);      // 添加定时器
    void adjust_timer(util_timer *timer);   // 调整定时器
    void del_timer(util_timer *timer);      // 删除定时器
    void tick();    // 定时任务处理函数

private:
    void add_timer(util_timer *timer, util_timer *lst_head);    // 辅助添加函数

    util_timer *head;   // 链表头
    util_timer *tail;   // 链表尾
};

// 通用类
class Utils{
public:
    Utils(){}
    ~Utils(){}

    // 静态函数避免this指针
    static void sig_handler(int sig);

    void init(int timeslot);
    int setnonblocking(int fd);
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);
    void addsig(int sig, void(*handler)(int), bool restart = true);
    void timer_handler();
    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;           // 本地套接字
    static int u_epollfd;           // epoll句柄
    sort_timer_lst m_timer_lst;     // 定时器链表
    int m_timeslot;                 // 定时时间
};

void cb_func(client_data *user_data);

#endif