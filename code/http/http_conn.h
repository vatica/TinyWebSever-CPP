#ifndef HTTP_CONN_H
#define HTTP_CONN_H

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
#include <cstring>
#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>
#include <stdarg.h>
#include <error.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../locker/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

class http_conn{
public:
    // 文件名称长度
    static const int FILENAME_LEN = 200;
    // 读缓冲大小
    static const int READ_BUFFER_SIZE = 2048;
    // 写缓冲大小
    static const int WRITE_BUFFER_SIZE = 1024;
    // 请求方法
    enum METHOD{
        GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATH
    };
    // 主状态机状态
    enum CHECK_STATE{
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    // 报文解析结果
    enum HTTP_CODE{
        NO_REQUEST = 0, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION
    };
    // 从状态机状态
    enum LINE_STATUS{
        LINE_OK = 0, LINE_BAD, LINE_OPEN
    };

public:
    http_conn(){}
    ~http_conn(){}

public:
    // 初始化连接
    void init(int sockfd, const sockaddr_in &addr, const char *, int, int, string user, string passwd, string sqlname);
    // 关闭连接
    void close_conn(bool real_close=true);
    void process();
    // 读取客户端全部数据
    bool read_once();
    // 写响应报文
    bool write();
    sockaddr_in* get_address(){
        return &m_address;
    }
    // 初始化数据库读取表
    void initmysql_result(connection_pool *connPool);
    int timer_flag;     // reactor是否处理数据
    int improv;         // reactor是否处理失败

private:
    void init();
    // 从m_read_buf读取，处理请求报文
    HTTP_CODE process_read();
    // 向m_write_buf写入响应报文
    bool process_write(HTTP_CODE ret);
    // 主状态机解析请求行数据
    HTTP_CODE parse_request_line(char *text);
    // 主状态机解析请求头数据
    HTTP_CODE parse_headers(char *text);
    // 主状态机解析请求内容
    HTTP_CODE parse_content(char *text);
    // 生成响应报文
    HTTP_CODE do_request();
    // 获得未解读数据位置
    //m_start_line是行在buffer中的起始位置，将该位置后面的数据赋给text
    //此时从状态机已提前将一行的末尾字符\r\n变为\0\0，所以text可以直接取出完整的行进行解析
    char* get_line(){return m_read_buf + m_start_line;};
    // 从状态机读取一行
    LINE_STATUS parse_line();
    void unmap();

    // 生成具体响应报文
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;       // epoll事件表
    static int m_user_count;    // 客户数量
    MYSQL *mysql;               // 数据库连接
    int m_state;                // reactor区分读写任务，0读，1写

private:
    int m_sockfd;               // 客户socket
    sockaddr_in m_address;      // 客户地址
    char m_read_buf[READ_BUFFER_SIZE];
    int m_read_idx;             // 已读数据结尾
    int m_checked_idx;          // 解析进行处
    int m_start_line;           // 解析开始处
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;            // 已写数据结尾
    CHECK_STATE m_check_state;  // 主状态机状态
    METHOD m_method;            // 请求方法

    // 解析请求报文变量
    char m_real_file[FILENAME_LEN]; // 实际文件路径
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;          // 是否为长连接

    char *m_file_address;   // 服务器上文件指针
    struct stat m_file_stat;// 文件信息结构体
    struct iovec m_iv[2];   // 向量元素
    int m_iv_count;         // 向量元素个数
    int cgi;                // 是否启用POST
    char *m_string;
    uint32_t bytes_to_send;      // 剩余发送字节
    uint32_t bytes_have_send;    // 已发送字节
    const char *doc_root;        // 资源根目录

    map<string, string> m_users;    // 用户表
    int m_TRIGMode;         // ET模式
    int m_close_log;        // 是否关闭日志

    char sql_user[100];     // 数据库用户名
    char sql_passwd[100];   // 数据库密码
    char sql_name[100];     // 数据库名
};

#endif