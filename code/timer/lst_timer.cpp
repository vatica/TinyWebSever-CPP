#include "lst_timer.h"
#include "../http/http_conn.h"

// 定时器链表构造函数
sort_timer_lst::sort_timer_lst(){
    head = nullptr;
    tail = nullptr;
}

// 定时器链表析构函数，析构所有定时器
sort_timer_lst::~sort_timer_lst(){
    util_timer *tmp = head;
    while(tmp){
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

// 添加定时器，按过期时间升序插入
void sort_timer_lst::add_timer(util_timer *timer){
    if(!timer)
        return ;
    if(!head){
        head = tail = timer;
        return ;
    }
    // 添加到链表头
    if(timer->expire < head->expire){
        timer->next = head;
        head->prev = timer;
        head = timer;
        return ;
    }
    // 添加到链表中
    add_timer(timer, head);
}

// 调整定时器
void sort_timer_lst::adjust_timer(util_timer *timer){
    if(!timer)
        return ;
    util_timer *tmp = timer->next;
    if(!tmp || (timer->expire < tmp->expire))
        return ;

    // 先从链表中删除，再重新按顺序插入
    if(timer == head){
        head = head->next;
        head->prev = nullptr;
        timer->next = nullptr;
        add_timer(timer, head);
    }
    else{
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

// 删除定时器
void sort_timer_lst::del_timer(util_timer *timer){
    if(!timer)
        return ;
    if((timer == head) && (timer == tail)){
        delete timer;
        head = nullptr;
        tail = nullptr;
        return ;
    }
    if(timer == head){
        head = head->next;
        head->prev = nullptr;
        delete timer;
        return ;
    }
    if(timer == tail){
        tail = tail->prev;
        tail->next = nullptr;
        delete timer;
        return ;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

// SIGALRM信号每次被触发，主循环中调用一次定时任务处理函数，处理链表容器中到期的定时器
void sort_timer_lst::tick(){
    if(!head)
        return ;
    time_t cur = time(nullptr);
    util_timer *tmp = head;
    while(tmp){
        if(cur < tmp->expire)
            break;
        // 过期时间小于当前时间，释放连接
        tmp->cb_func(tmp->user_data);
        head = tmp->next;
        if(head)
            head->prev = nullptr;
        delete tmp;
        tmp = head;
    }
}

// 添加定时器，从给定head开始寻找可以插入的位置
void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head){
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;
    while(tmp){
        if(timer->expire < tmp->expire){
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if(!tmp){
        prev->next = timer;
        timer->prev = prev;
        timer->next = nullptr;
        tail = timer;
    }
}

void Utils::init(int timeslot){
    m_timeslot = timeslot;
}

// 设置非阻塞，读不到数据时返回-1，并且设置errno为EAGAIN
int Utils::setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 向epoll注册事件
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;
    // ET模式
    if(TRIGMode == 1)
        // EPOLLRDHUP对端关闭
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    // 默认LT模式
    else
        event.events = EPOLLIN | EPOLLRDHUP;
    // 只触发一次
    if(one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    // 设置非阻塞
    setnonblocking(fd);
}

// 信号处理函数
void Utils::sig_handler(int sig){
    // 为保证函数的可重入性，保留原来的errno
    // 可重入性表示中断后再次进入该函数，环境变量与之前相同，不会丢失数据
    int save_errno = errno;
    int msg = sig;

    // 将信号从管道写端写入，以字符类型传输
    // 回到主循环处理信号业务逻辑
    send(u_pipefd[1], (char *)&msg, 1, 0);

    // 恢复errno
    errno = save_errno;
}

// 设置信号
void Utils::addsig(int sig, void (*handler)(int), bool restart){
    // 创建sigaction结构体
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    // 设置信号处理函数
    sa.sa_handler = handler;
    if(restart)
        // 使被信号打断的系统调用重新自动发起
        sa.sa_flags |= SA_RESTART;
    // 对信号集初始化，将所有信号添加到信号集中
    sigfillset(&sa.sa_mask);

    // 设置信号，-1表示有错误发生
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 定时器触发
void Utils::timer_handler(){
    // 处理连接链表
    m_timer_lst.tick();
    // 重新设置时钟
    alarm(m_timeslot);
}

void Utils::show_error(int connfd, const char *info){
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

// 定时器回调函数，释放客户端连接
void cb_func(client_data *user_data){
    // 删除非活动连接在socket上的注册事件
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);

    // 关闭socket
    close(user_data->sockfd);

    // 减少连接数
    http_conn::m_user_count--;
}