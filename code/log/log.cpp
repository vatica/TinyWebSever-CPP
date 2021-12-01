#include <cstring>
#include <ctime>
#include <sys/time.h>
#include <stdarg.h>
#include <pthread.h>
#include "log.h"

using namespace std;

Log::Log(){
    m_count = 0;
    m_is_async = false;
}

Log::~Log(){
    if(m_fp != nullptr)
        fclose(m_fp);
}

// 初始化日志，异步需要设置阻塞队列的长度，同步不需要
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size){
    // 如果设置了max_queue_size，则设置为异步
    if(max_queue_size >= 1){
        m_is_async = true;
        m_log_queue = new block_queue<string>(max_queue_size);
        // 创建线程异步写
        pthread_t tid;
        pthread_create(&tid, nullptr, flush_log_thread, nullptr);
    }

    // 初始化日志
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, 0, m_log_buf_size);
    m_split_lines = split_lines;

    // 获取当前时间
    time_t t = time(nullptr);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    // 查找字符从右面开始第一次出现的位置，截断文件名
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};
    
    // 构造日志文件名
    if(p == nullptr){
        // 将可变参数格式化到字符串中
        // 没有/则直接在当前路径下
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else{
        strcpy(log_name, p+1);  // 日志文件名
        strncpy(dir_name, file_name, p-file_name+1);    // 日志路径
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;

    // 追加写
    m_fp = fopen(log_full_name, "a");
    if(m_fp == nullptr)
        return false;
    return true;
}

// 写日志
void Log::write_log(int level, const char *format, ...){
    // 秒、微妙
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    // 获取时间结构体
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    // 临界区加锁
    m_mutex.lock();

    m_count++;
    // 新的一天或者日志达到最大行数，需要更换日志文件
    if(m_today != my_tm.tm_mday || m_count % m_split_lines == 0){
        // 刷新文件缓冲并关闭文件
        fflush(m_fp);
        fclose(m_fp);

        // 新日志路径
        char new_log[256] = {0};
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        // 天数变化
        if(m_today != my_tm.tm_mday){
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        // 日志达到最大行数
        else{
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }
        // 打开新日志
        m_fp = fopen(new_log, "a");
    }

    m_mutex.unlock();
    
    // 日志级别
    char s[16] = {0};
    switch(level){
        case 0: strcpy(s, "[debug]:"); break;
        case 1: strcpy(s, "[info]:"); break;
        case 2: strcpy(s, "[warn]:"); break;
        case 3: strcpy(s, "[erro]:"); break;
        default: strcpy(s, "[debug]:"); break;
    }

    // 临界区加锁
    m_mutex.lock();

    // 构造日志内容
    // 时间、级别
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                    my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                    my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    // 正文
    va_list valst;
    va_start(valst, format);
    int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valst);
    va_end(valst); 

    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';

    // 异步写
    if(m_is_async && !m_log_queue->full()){
        m_log_queue->push(m_buf);
    }
    // 同步写
    else{
        fputs(m_buf, m_fp);
    }

    m_mutex.unlock();
}

// 强制刷新文件缓冲
void Log::flush(void){
    m_mutex.lock();
    fflush(m_fp);
    m_mutex.unlock();
}