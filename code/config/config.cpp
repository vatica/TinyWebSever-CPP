#include "config.h"

// 解析输入参数
void Config::parse_arg(int argc, char*argv[]){
    int opt;
    const char *str = "p:c:l:s:t:a:m:o:";   // 一个冒号表示必有一个参数
    while((opt = getopt(argc, argv, str)) != -1){
        switch (opt){
            case 'p': port = atoi(optarg); break;
            case 'c': close_log = atoi(optarg); break;
            case 'l': async_log = atoi(optarg); break;
            case 's': sql_num = atoi(optarg); break;
            case 't': thread_num = atoi(optarg); break;
            case 'a': actor_model = atoi(optarg); break;
            case 'm': trig_mode = atoi(optarg); break;
            case 'o': opt_linger = atoi(optarg); break;
            default: throw invalid_argument("check your arguments"); break;
        }
    }
}