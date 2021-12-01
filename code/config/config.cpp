#include "config.h"

Config::Config(){
    // 端口号，默认8081
    port = 8081;

    // 日志写入方式，默认同步，1异步
    log_write = 0;

    // 触发组合模式，默认listenfd LT + connfd LT，1 LT + ET，2 ET + LT，3 ET + ET
    trig_mode = 0;

    // 优雅关闭链接，默认不使用，1使用
    opt_linger = 0;

    // 数据库连接池数量，默认8
    sql_num = 8;

    // 线程池内的线程数量，默认8
    thread_num = 8;

    // 关闭日志，默认不关闭，1关闭
    close_log = 0;

    // 并发模型，默认是proactor，1reactor
    actor_model = 0;
}

// 解析输入参数
void Config::parse_arg(int argc, char*argv[]){
    int opt;
    const char *str = "p:l:m:o:s:t:c:a:";   // 一个冒号表示必有一个参数
    while((opt = getopt(argc, argv, str)) != -1){
        switch (opt){
            case 'p': port = atoi(optarg); break;
            case 'l': log_write = atoi(optarg); break;
            case 'm': trig_mode = atoi(optarg); break;
            case 'o': opt_linger = atoi(optarg); break;
            case 's': sql_num = atoi(optarg); break;
            case 't': thread_num = atoi(optarg); break;
            case 'c': close_log = atoi(optarg); break;
            case 'a': actor_model = atoi(optarg); break;
            default: throw invalid_argument("check your arguments"); break;
        }
    }
}