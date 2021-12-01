#ifndef CONFIG_H
#define CONFIG_H

#include <unistd.h>
#include <stdexcept>
#include <string>

using namespace std;

class Config{
public:
    Config(){}
    ~Config(){}

    void parse_arg(int argc, char *argv[]);
    
public:
    string user = "root";                   // 数据库用户名
    string password = "4869";               // 数据库密码
    string database_name = "webserver";     // 数据库名
    char *root = "./resource";              // 资源根目录
    
    int port = 8081;        // 端口号，默认8081
    int close_log = 0;      // 关闭日志，默认不关闭，1关闭
    int async_log = 0;      // 日志写入方式，默认同步，1异步
    int sql_num = 8;        // 数据库连接池数量，默认8
    int thread_num = 8;     // 线程池内的线程数量，默认8
    int actor_model = 0;    // 并发模型，默认proactor，1reactor
    int trig_mode = 0;      // 触发组合模式，默认listenfd LT + connfd LT，1 LT + ET，2 ET + LT，3 ET + ET
    int opt_linger = 0;     // 优雅关闭链接，默认不使用，1使用    
};

#endif