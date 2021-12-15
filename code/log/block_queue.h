#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <sys/time.h>
#include "../locker/locker.h"

using namespace std;

// 阻塞队列模板类
template <class T>
class block_queue{
public:
    block_queue(int max_size = 1000){
        if(max_size <= 0){
            exit(-1);
        }
        m_max_size = max_size;
        m_array = new T[max_size];
        m_size = 0;
        m_front = 1;
        m_back = -1;
    }

    void clear(){
        m_mutex.lock();
        m_size = 0;
        m_front = -1;
        m_back = -1;
        m_mutex.unlock();
    }

    ~block_queue(){
        m_mutex.lock();
        if(m_array != nullptr)
            delete [] m_array;
        m_mutex.unlock();
    }

    // 判断队列是否已满
    bool full(){
        m_mutex.lock();
        if(m_size >= m_max_size){
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    // 判断队列是否为空
    bool empty(){
        m_mutex.lock();
        if(m_size == 0){
            m_mutex.lock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    // 返回队首元素
    bool front(T &value){
        m_mutex.lock();
        if(m_size == 0){
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }

    // 返回队尾元素
    bool back(T &value){
        m_mutex.lock();
        if(m_size == 0){
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_back];
        return true;
    }

    int size(){
        int tmp = 0;
        m_mutex.lock();
        tmp = m_size;
        m_mutex.unlock();
        return tmp;
    }

    int max_size(){
        int tmp = 0;
        m_mutex.lock();
        tmp = m_max_size;
        m_mutex.unlock();
        return tmp;
    }

    // 添加元素
    bool push(const T &item){
        m_mutex.lock();
        if(m_size >= m_max_size){
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }
        // 循环队列
        m_back = (m_back + 1) % m_max_size;
        m_array[m_back] = item;
        m_size++;

        // 广播唤醒，线程竞争
        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }

    // 取第一个元素
    bool pop(T &item){
        m_mutex.lock();
        // 多个线程竞争资源，wait成功返回不一定还有资源，所以使用while每次检查
        while(m_size <= 0){
            if(!m_cond.wait(m_mutex.get())){
                m_mutex.unlock();
                return false;
            }
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }

    // 增加超时处理
    bool pop(T &item, int ms_timeout){
        struct timespec t = {0, 0};     // s and ns
        struct timeval now = {0, 0};    // s and ms
        // 格林威治时间
        gettimeofday(&now, nullptr);

        m_mutex.lock();
        // 把while拆成两个if，只等待一次
        if(m_size <= 0){
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if(!m_cond.timewait(m_mutex.get(), t)){
                m_mutex.unlock();
                return false;
            }
        }
        // 防止资源已经被使用
        if(m_size <= 0){
            m_mutex.unlock();
            return false;
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }

private:
    mutexlocker m_mutex;
    condvar m_cond;

    T *m_array;
    int m_size;
    int m_max_size;
    int m_front;
    int m_back;
};

#endif