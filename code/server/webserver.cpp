#include "webserver.h"

WebServer::WebServer(string user, string password, string database_name, const char *root,
                     int port, int close_log, int async_log, int sql_num,
                     int thread_num, int actor_model, int trig_mode, int opt_linger){
    m_user = user;
    m_passWord = password;
    m_databaseName = database_name;
    m_root = root;
    m_port = port;
    m_close_log = close_log;
    m_async_log = async_log;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_actormodel = actor_model;
    m_listen_trig_mode = trig_mode & 2;
    m_conn_trig_mode = trig_mode & 1;
    m_opt_linger = opt_linger;
    
    users = new http_conn[MAX_FD];
    users_timer = new client_data[MAX_FD];
    log_write();
    sql_pool();
    thread_pool();
}

WebServer::~WebServer(){
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
    delete m_connPool;
}

// 初始化日志
void WebServer::log_write(){
    if(m_close_log == 0){
        // 获取单例并初始化
        if(m_async_log == 1)
            // 异步写，需要阻塞队列长度
            Log::get_instance()->init("./log/ServerLog", 2000, 800000, 800);
        else
            // 同步写
            Log::get_instance()->init("./log/ServerLog", 2000, 800000, 0);
    }
}

// 初始化数据库连接池
void WebServer::sql_pool(){
    m_connPool = connection_pool::get_instance();
    m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_num, m_close_log);

    // 读取用户表
    users->initmysql_result(m_connPool);
}

// 初始化线程池
void WebServer::thread_pool(){
    m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

// 监听事件，网络编程基础步骤
void WebServer::eventListen(){
    // ipv4，面向连接
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    // 优雅关闭连接
    // 缺省为close()继续发送缓冲区残留数据，等待确认然后返回
    // 将修改为close()设置一个超时，发送缓冲区残留数据，全部确认则正常关闭，否则发送RST、丢弃数据并跳过time_wait直接关闭
    if(m_opt_linger){
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    // 重用端口
    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    // 转网络字节序
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    // 绑定ip和端口
    int ret = 0;
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    // 监听
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    // 参数没有意义
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    utils.init(TIMESLOT);
    // 注册事件并设置非阻塞
    utils.addfd(m_epollfd, m_listenfd, false, m_listen_trig_mode);
    http_conn::m_epollfd = m_epollfd;

    // 创建一对套接字，fd[1]写，fd[0]读
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);
    // 统一事件源
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    // 两次向已关闭的连接发送数据导致SIGPIPE，避免进程退出，捕获SIGPIPE并忽略
    utils.addsig(SIGPIPE, SIG_IGN);
    // 时钟信号
    utils.addsig(SIGALRM, utils.sig_handler, false);
    // kill终止信号
    utils.addsig(SIGTERM, utils.sig_handler, false);
    // abort终止信号
    utils.addsig(SIGABRT, utils.sig_handler, false);

    // 定时器
    alarm(TIMESLOT);

    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

// 设置客户和定时器
void WebServer::timer(int connfd, struct sockaddr_in client_address){
    // 初始化客户
    users[connfd].init(connfd, client_address, m_root, m_conn_trig_mode, m_close_log, m_user, m_passWord, m_databaseName);

    // 创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer);
}

//若有数据传输，则将定时器往后延迟3个单位
//并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(util_timer *timer){
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer once");
}

// 释放连接，删除计时器
void WebServer::deal_timer(util_timer *timer, int sockfd){
    timer->cb_func(&users_timer[sockfd]);
    if(timer){
        utils.m_timer_lst.del_timer(timer);
    }

    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

// 接受新客户连接
bool WebServer::dealclinetdata(){
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if(m_listen_trig_mode == 0){
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if(connfd < 0){
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if(http_conn::m_user_count >= MAX_FD){
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address);
    }
    else{
        while(1){
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if(connfd < 0){
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if(http_conn::m_user_count >= MAX_FD){
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

// 处理信号
bool WebServer::dealwithsignal(bool &timeout, bool &stop_server){
    int ret = 0;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if(ret == -1){
        return false;
    }
    else if(ret == 0){
        return false;
    }
    else{
        for(int i = 0; i < ret; ++i){
            switch(signals[i]){
                case SIGALRM: timeout = true; break;
                case SIGTERM: stop_server = true; break;
                case SIGABRT: stop_server = true; break;
            }
        }
    }
    return true;
}

// 处理读
void WebServer::dealwithread(int sockfd){
    util_timer *timer = users_timer[sockfd].timer;

    // reactor
    if(m_actormodel == 1){
        // 更新计时器
        if(timer){
            adjust_timer(timer);
        }

        // 若监测到读事件，将该事件放入请求队列
        m_pool->append(users + sockfd, 0);

        // 等待工作线程读
        while(true){
            if(users[sockfd].improv == 1){
                // 读失败
                if(users[sockfd].timer_flag == 1){
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    // proactor
    else{
        if(users[sockfd].read_once()){
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            // 更新计时器
            if(timer){
                adjust_timer(timer);
            }

            // 若监测到读事件，将该事件放入请求队列
            m_pool->append_p(users + sockfd);  
        }
        // 读失败
        else{
            deal_timer(timer, sockfd);
        }
    }
}

// 处理写
void WebServer::dealwithwrite(int sockfd){
    util_timer *timer = users_timer[sockfd].timer;
    
    // reactor
    if(m_actormodel == 1){
        if(timer){
            adjust_timer(timer);
        }

        m_pool->append(users + sockfd, 1);

        while(true){
            if(users[sockfd].improv == 1){
                if(users[sockfd].timer_flag == 1){
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else{
        // proactor
        if(users[sockfd].write()){
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if(timer){
                adjust_timer(timer);
            }
        }
        else{
            deal_timer(timer, sockfd);
        }
    }
}

// 循环处理事件
void WebServer::eventLoop(){
    bool timeout = false;
    bool stop_server = false;

    while(!stop_server){
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        // epoll_wait会被信号打断，返回-1并设置errno=EINTR
        if(number < 0 && errno != EINTR){
            LOG_ERROR("%s", "epoll failure");
            break;
        }
        // 遍历事件
        for(int i = 0; i < number; i++){
            int sockfd = events[i].data.fd;
            // 处理新到的客户连接
            if(sockfd == m_listenfd){
                bool flag = dealclinetdata();
                if(flag == false)
                    continue;
            }
            // 对端关闭
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                // 服务器端关闭连接，移除对应的定时器
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            // 处理信号
            else if((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)){
                bool flag = dealwithsignal(timeout, stop_server);
                if(flag == false)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
            // 处理客户连接上接收到的数据
            else if(events[i].events & EPOLLIN){
                dealwithread(sockfd);
            }
            // 处理客户连接上要发送的数据
            else if(events[i].events & EPOLLOUT){
                dealwithwrite(sockfd);
            }
        }
        // 时钟到时
        if(timeout){
            utils.timer_handler();
            LOG_INFO("%s", "timer tick");
            timeout = false;
        }
    }
}