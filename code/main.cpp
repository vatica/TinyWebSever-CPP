#include "./config/config.h"
#include "./server/webserver.h"

int main(int argc, char *argv[]){
    // 数据库用户名、密码、库名
    string user = "root";
    string password = "4869";
    string database_name = "webserver";

    //命令行解析
    Config config;
    config.parse_arg(argc, argv);

    WebServer server;

    //初始化
    server.init(config.port, user, password, database_name, config.log_write, 
                config.opt_linger, config.trig_mode, config.sql_num, config.thread_num, 
                config.close_log, config.actor_model);
    
    //日志
    server.log_write();

    //数据库
    server.sql_pool();

    //线程池
    server.thread_pool();

    //触发模式
    server.trig_mode();

    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;
}