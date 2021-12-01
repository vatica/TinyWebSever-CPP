#include "./config/config.h"
#include "./server/webserver.h"

int main(int argc, char *argv[]){
    Config config;
    config.parse_arg(argc, argv);

    WebServer server(config.user, config.password, config.database_name, config.root,
                     config.port, config.close_log, config.async_log, config.sql_num,
                     config.thread_num, config.actor_model, config.trig_mode, config.opt_linger);
    server.eventListen();
    server.eventLoop();

    return 0;
}