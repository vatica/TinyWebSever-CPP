#ifndef LOG_H
#define LOG_H

#include <cstdio>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

using namespace std;

class Log{
public:
    // c++11后，局部静态变量懒汉式不用加锁
    static Log* get_instance(){
        static Log instance;
        return &instance;
    }

    bool init(const char *file_name, int close_log, int log_buf_size=8192, int split_lines=5000000, int max_queue_size=0);
    void write_log(int level, const char *format, ...);
    void flush(void);

private:
    Log();
    ~Log();

    // 线程工作函数，静态函数防止产生this指针
    static void* flush_log_thread(void *args){
        Log::get_instance()->async_write_log();
    }

    // 异步写
    void async_write_log(){
        string single_log;
        // 从阻塞队列取出一个日志
        while(m_log_queue->pop(single_log)){
            m_mutex.lock();
            // 写入后文件指针后移
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }

private:
    char dir_name[128]; // 路径名
    char log_name[128]; // log文件名
    int m_split_lines;  // 日志最大行数
    int m_log_buf_size; // 日志缓冲区大小
    long long m_count;  // 日志记录行数
    int m_today;        // 日志日期
    FILE *m_fp;         // 日志文件指针
    char *m_buf;        // 缓冲区
    block_queue<string> *m_log_queue;   //阻塞队列
    bool m_is_async;    // 是否异步
    mutexlocker m_mutex;
};

// 可变参数宏__VA_ARGS__，当可变参数个数为0时，##__VA_ARGS__把前面多余的“，”去掉
#define LOG_DEBUG(format, ...) if(m_close_log == 0) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(m_close_log == 0) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(m_close_log == 0) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(m_close_log == 0) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif