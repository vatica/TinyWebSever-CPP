#include <mysql/mysql.h>
#include <fstream>

#include "http_conn.h"

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Yor request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

mutexlocker m_lock;         // 表互斥锁
map<string, string> users;  // 内存用户表

void http_conn::initmysql_result(connection_pool *connPool){
    // 从数据库连接池取一个连接
    MYSQL *mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connPool);

    // 在user表中检索
    if(mysql_query(mysql, "SELECT username, password FROM user")){
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    // 从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    // 结果集的列数
    // int num_fields = mysql_num_fields(result);

    // 所有字段结构的数组
    // MYSQL_FIELD *fields = mysql_fetch_field(result);

    // 将用户名和密码存入map中
    while(MYSQL_ROW row = mysql_fetch_row(result)){
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

// 对文件描述符设置非阻塞
int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    if(TRIGMode == 1)
        // 读事件、ET模式、对方断开连接
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if(one_shot)
        event.events |= EPOLLONESHOT;

    // 注册事件
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 从内核事件表删除描述符，关闭描述符
void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    if(TRIGMode == 1)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

// 关闭一个客户连接
void http_conn::close_conn(bool real_close){
    if(real_close && (m_sockfd != -1)){
        printf("close %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

// 初始化连接
void http_conn::init(int sockfd, const sockaddr_in &addr, const char *root, int TRIGMode, 
                     int close_log, string user, string passwd, string sqlname){
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true, m_TRIGMode);
    m_user_count++;

    doc_root = root;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();
}

// 初始化新接受的连接
void http_conn::init(){
    mysql = nullptr;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, 0, READ_BUFFER_SIZE);
    memset(m_write_buf, 0, WRITE_BUFFER_SIZE);
    memset(m_real_file, 0, FILENAME_LEN);
}

// 从状态机，用于分析一行内容
http_conn::LINE_STATUS http_conn::parse_line(){
    char temp;
    //m_read_idx指向缓冲区m_read_buf的数据末尾的下一个字节
    //m_checked_idx指向从状态机当前正在分析的字节
    for(; m_checked_idx < m_read_idx; ++m_checked_idx){
        temp = m_read_buf[m_checked_idx];
        // 可能读取到完整行
        if(temp == '\r'){
            // 不完整
            if((m_checked_idx + 1) == m_read_idx){
                return LINE_OPEN;
            }
            // 完整，\r\n替换为\0\0
            else if(m_read_buf[m_checked_idx + 1] == '\n'){
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            // 格式错误
            return LINE_BAD;
        }
        // 可能读取到完整行
        else if(temp == '\n'){
            // 完整，\r\n替换为\0\0
            if(m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r'){
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    // 继续接收
    return LINE_OPEN;
}

// 循环读取客户数据，直到无数据可读或对方关闭连接
bool http_conn::read_once(){
    if(m_read_idx >= READ_BUFFER_SIZE){
        return false;
    }
    int bytes_read = 0;

    // LT模式
    if(m_TRIGMode == 0){
        // 读取数据到缓冲区
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        m_read_idx += bytes_read;

        if(bytes_read <= 0){
            return false;
        }
        return true;
    }
    // ET模式
    else{
        while(true){
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            // 无数据可读
            if(bytes_read == -1){
                // 非阻塞、连接正常
                if(errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return false;
            }
            // 另一端已关闭
            else if(bytes_read == 0){
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }
}

// 解析HTTP请求行，获得请求方法、目标url及http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char *text){
    // 寻找空格和\t位置
    m_url = strpbrk(text, " \t");
    if(!m_url){
        return BAD_REQUEST;
    }
    // 将该位置改为\0，方便取出
    *m_url++ = '\0';
    // 取出请求方法并比较
    char *method = text;
    if(strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if(strcasecmp(method, "POST") == 0){
        m_method = POST;
        cgi = 1;
    }
    else return BAD_REQUEST;

    // 跳过空格和\t
    m_url += strspn(m_url, " \t");
    // 再寻找空格和\t
    m_version = strpbrk(m_url, " \t");
    if(!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    // 跳过空格和\t
    m_version += strspn(m_version, " \t");

    // 仅支持HTTP1.1
    if(strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    // 去除http://
    if(strncasecmp(m_url, "http://", 7) == 0){
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    // 去除https://
    if(strncasecmp(m_url, "https://", 8) == 0){
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if(!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    // url为/，显示主页
    if(strlen(m_url) == 1)
        strcat(m_url, "judge.html");

    // 主状态机状态修改为处理请求头
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

// 解析HTTP请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char *text){
    // 首位为\0是空行
    if(text[0] == '\0'){
        if(m_content_length != 0){
            // POST继续解析消息体
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        // GET请求解析完成
        return GET_REQUEST;
    }
    // 连接字段
    else if(strncasecmp(text, "connection:", 11) == 0){
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0){
            // 长连接
            m_linger = true;
        }
    }
    // 内容长度字段
    else if(strncasecmp(text, "Content-length:", 15) == 0){
        text += 15;
        text += strspn(text, "\t");
        m_content_length = atol(text);
    }
    // Host字段，请求站点
    else if(strncasecmp(text, "Host:", 5) == 0){
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else{
        LOG_INFO("oop! unknow header: %s", text);
    }
    return NO_REQUEST;
}

// 判断HTTP请求是否被完全读入
http_conn::HTTP_CODE http_conn::parse_content(char *text){
    // 判断buffer中是否读取了消息体
    if(m_read_idx >= (m_content_length + m_checked_idx)){
        // 最后填充\0
        text[m_content_length] = '\0';
        // 获取消息体
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read()
{
    // 初始化状态
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = nullptr;

    // 消息体末尾没有字符，POST请求报文不能用从状态机LINE_OK状态判断
    // 加上&& line_status == LINE_OK防止陷入死循环
    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK)){
        // 取一行数据
        text = get_line();
        m_start_line = m_checked_idx;
        LOG_INFO("get line: %s", text);

        // 主状态机状态
        switch (m_check_state){
            case CHECK_STATE_REQUESTLINE:{
                ret = parse_request_line(text);
                if(ret == BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER:{
                ret = parse_headers(text);
                if(ret == BAD_REQUEST)
                    return BAD_REQUEST;
                // 完整解析GET请求
                else if(ret == GET_REQUEST)
                    return do_request();
                break;
            }
            case CHECK_STATE_CONTENT:{
                ret = parse_content(text);
                // 完整解析POST请求
                if(ret == GET_REQUEST)
                    return do_request();
                // 完成报文解析，防止进入循环
                line_status = LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request(){
    // 网站根目录
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);

    // 找到最后一个/的位置
    const char *p = strrchr(m_url, '/');

    // POST请求，实现登录和注册校验
    if(cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        // 将用户名和密码提取出来
        // user=123&password=123
        char name[100], password[100];
        int i;
        for(i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for(i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        if(*(p + 1) == '3'){
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, password) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if(users.find(name) == users.end()){
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string, string>(name, password));
                m_lock.unlock();

                if(!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html");
        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if(*(p + 1) == '2')
        {
            if(users.find(name) != users.end() && users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
    }
    // GET请求，跳转到注册页面
    if(*(p + 1) == '0'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // GET请求，跳转到登录页面
    else if(*(p + 1) == '1'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // POST请求，图片页面
    else if(*(p + 1) == '5'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // POST请求，视频页面
    else if(*(p + 1) == '6'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // POST请求，关注页面
    else if(*(p + 1) == '7'){
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        // 都不是则直接拼接
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    // 获取不到文件信息，资源不存在
    if(stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;

    // 文件是否可读
    if(!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    // 文件是否为目录
    if(S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    // 以只读方式打开文件并映射到内存中
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    // 关闭文件描述符
    close(fd);
    return FILE_REQUEST;
}

// 关闭文件映射
void http_conn::unmap(){
    if(m_file_address){
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

// 向客户端写响应
bool http_conn::write(){
    int temp = 0;

    // 响应报文为空
    if(bytes_to_send == 0){
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }

    while(1){
        // 发送响应
        temp = writev(m_sockfd, m_iv, m_iv_count);

        if(temp < 0){
            // 重试，继续监听写事件
            if(errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                // 不要断开连接
                return true;
            }
            // 断开连接
            unmap();
            return false;
        }

        // 正常发送
        bytes_have_send += temp;
        bytes_to_send -= temp;
        // 第一个元素已经发送完
        if(bytes_have_send >= m_iv[0].iov_len){
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        // 继续发送第一个元素
        else{
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        // 全部发送完毕
        if(bytes_to_send <= 0){
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

            // 长连接
            if(m_linger){
                init();
                return true;
            }
            else{
                return false;
            }
        }
    }
}

// 写响应
bool http_conn::add_response(const char *format, ...){
    // 写入内容超过buffer长度则报错
    if(m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    
    // 可变参数列表
    va_list arg_list;
    // 初始化列表
    va_start(arg_list, format);

    // 按照format写入缓存
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    // 超出缓存则报错
    if(len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)){
        va_end(arg_list);
        return false;
    }

    // 更新idx
    m_write_idx += len;
    va_end(arg_list);

    LOG_INFO("add response: %s", m_write_buf);
    return true;
}

// 状态行
bool http_conn::add_status_line(int status, const char *title){
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

// 响应报头
bool http_conn::add_headers(int content_len){
    return add_content_length(content_len) && add_linger() &&
           add_blank_line();
}

// 消息报头
bool http_conn::add_content_length(int content_len){
    return add_response("Content-Length:%d\r\n", content_len);
}

bool http_conn::add_content_type(){
    return add_response("Content-Type:%s\r\n", "text/html");
}

bool http_conn::add_linger(){
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

// 空行
bool http_conn::add_blank_line(){
    return add_response("%s", "\r\n");
}

// 响应正文
bool http_conn::add_content(const char *content){
    return add_response("%s", content);
}

// 向缓冲区写响应
bool http_conn::process_write(HTTP_CODE ret){
    switch (ret){
        case INTERNAL_ERROR:{
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if(!add_content(error_500_form))
                return false;
            break;
        }
        case BAD_REQUEST:{
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if(!add_content(error_404_form))
                return false;
            break;
        }
        case FORBIDDEN_REQUEST:{
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if(!add_content(error_403_form))
                return false;
            break;
        }
        case FILE_REQUEST:{
            add_status_line(200, ok_200_title);
            if(m_file_stat.st_size != 0){
                add_headers(m_file_stat.st_size);
                // 第一个元素指向响应报文写缓冲
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                // 第二个元素指向mmap返回的文件指针
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;
            }
            // 资源大小为0则返回空白html
            else{
                const char *ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                if(!add_content(ok_string))
                    return false;
            }
        }
        default:
            return false;
    }
    // 除FILE_REQUEST外只指向响应报文缓冲
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

// http处理
void http_conn::process(){
    HTTP_CODE read_ret = process_read();
    // 请求不完整，继续注册读事件
    if(read_ret == NO_REQUEST){
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    bool write_ret = process_write(read_ret);
    if(!write_ret){
        close_conn();
    }
    // 准备好写缓冲，加入监听可写事件
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}