#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>         // 请求队列
#include <cstdio>
#include <exception>    // std异常
#include <pthread.h>    // 线程函数
#include "../locker/locker.h"                   // 线程同步锁封装类
#include "../CGImysql/sql_connection_pool.h"    // 数据库

// 线程池模板类
template <typename T>
class threadpool{
public:
    // thread_number线程池中线程的数量
    // connPool是数据库连接池指针
    // max_request队列中最大请求数量
    threadpool(int actor_model, connection_pool *connPool, uint32_t thread_number, uint32_t max_request = 10000);
    ~threadpool();
    // 添加请求
    bool append(T *request, int state);
    bool append_p(T *request);

private:
    // 工作线程运行函数，需要是静态函数，因为pthread_create()第三个参数是(void *)，而成员函数会编译为带有this指针参数，从而不能匹配
    // 静态成员函数没有this指针，但不能访问非静态成员变量，因此在内部调用run
    static void* worker(void *arg);
    void run();

private:
    int m_actor_model;              // 处理模式，1 reactor，0 proactor
    connection_pool *m_connPool;    // 数据库连接
    uint32_t m_thread_number;       // 线程池中的最大线程数
    uint32_t m_max_request;         // 请求队列中最大请求数
    pthread_t *m_threads;           // 线程数组，大小为m_thread_number
    std::list<T*> m_workqueue;      // 请求队列
    mutexlocker m_queuelocker;      // 请求队列互斥锁
    semaphore m_queuestat;          // 是否有任务需要处理信号量
};

// 构造线程池，创建线程
// 类成员函数参数默认值只在定义或声明其中一处对同一个参数设置
template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPoll, uint32_t thread_number, uint32_t max_request)
:m_actor_model(actor_model), m_connPool(connPoll), m_thread_number(thread_number), m_max_request(max_request), m_threads(nullptr){
    if(thread_number <= 0 || max_request <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads)
        throw std::exception();
    for(uint32_t i = 0; i < thread_number; ++i){
        // 内存单元、线程属性（默认NULL）、工作函数、传递参数（线程池）
        if(pthread_create(m_threads + i, NULL, worker, this) != 0){
            delete[] m_threads;
            throw std::exception();
        }
        // 分离主线程与子线程，子线程结束后资源自动回收
        if(pthread_detach(m_threads[i]) != 0){
            delete[] m_threads;
            throw std::exception();
        }
    }
}

// 析构，释放线程数组
template <typename T>
threadpool<T>::~threadpool(){
    delete[] m_threads;
}

// 添加请求，并利用信号量通知工作线程
template <typename T>
bool threadpool<T>::append(T *request, int state){
    // 任务队列是临界区，加互斥锁
    m_queuelocker.lock();
    if(m_workqueue.size() >= m_max_request){
        m_queuelocker.unlock();
        return false;
    }
    // 加入请求队列，然后解锁
    request->m_state = state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    // 增加信号量，表示有任务要处理
    m_queuestat.post();
    return true;
}

// 添加请求，不带状态
template <typename T>
bool threadpool<T>::append_p(T *request){
    m_queuelocker.lock();
    if(m_workqueue.size() >= m_max_request){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

// 工作线程运行
template <typename T>
void* threadpool<T>::worker(void *arg){
    // 转换为线程池类
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return nullptr;
}

// 工作函数
template <typename T>
void threadpool<T>::run(){
    while(true){
        // 信号量阻塞，等待任务
        m_queuestat.wait();
        // 请求队列临界区，加互斥锁
        m_queuelocker.lock();
        if(m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;
        }
        // 从请求队列取第一个请求
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request)
            continue;
        // reactor
        if(m_actor_model == 1){
            // 读
            if(request->m_state == 0){
                if(request->read_once()){
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else{
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            // 写
            else{
                if(request->write()){
                    request->improv = 1;
                }
                else{
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        // proactor
        else{
            // 从连接池中取出一个数据库连接 RAII
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            // 处理请求
            request->process();
        }
    }
}

#endif