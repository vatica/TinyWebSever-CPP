#ifndef LOCKER_H
#define LOCKER_H

#include <exception>    // std异常
#include <pthread.h>    // Posix线程接口，提供互斥锁、条件变量
#include <semaphore.h>  // Posix信号量

/*
信号量封装类

初始化信号量
int sem_init(sem_t *sem, int pshared, unsigned int value);
    sem 信号量对象
    pshared 不为0信号量在进程间共享，否则在当前进程的所有线程共享
    value 信号量值大小
    成功返回0，失败返回-1并设置errno

原子操作V
int sem_wait(sem_t *sem);
    信号量不为0则-1，为0则阻塞
    成功返回0，失败返回-1并设置errno

原子操作P
int sem_post(sem_t *sem);
    信号量+1
    成功返回0，失败返回-1并设置errno

销毁信号量
int sem_destroy(sem_t *sem);
    成功返回0，失败返回-1并设置errno
*/
class semaphore{
public:
    semaphore(){
        if(sem_init(&m_sem, 0, 0) != 0){
            throw std::exception();
        }
    }
    
    semaphore(int num){
        if(sem_init(&m_sem, 0, num) != 0){
            throw std::exception();
        }
    }

    ~semaphore(){
        sem_destroy(&m_sem);
    }

    bool wait(){
        return sem_wait(&m_sem) == 0;
    }

    bool post(){
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};


/*
互斥锁封装类

初始化互斥锁
int pthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr);
    mutex 互斥锁对象
    attr 互斥锁属性，默认普通锁
    value信号量值大小
    成功返回0

加锁
int pthread_mutex_lock (pthread_mutex_t *mutex);
    请求锁线程形成等待序列，解锁后按优先级获得锁
    成功返回0

解锁
int pthread_mutex_unlock (pthread_mutex_t *mutex);
    成功返回0

销毁互斥锁
int pthread_mutex_destroy (pthread_mutex_t *mutex);
    成功返回0
*/
class mutexlocker{
public:
    mutexlocker(){
        if(pthread_mutex_init(&m_mutex, NULL) != 0){
            throw std::exception();
        }
    }

    ~mutexlocker(){
        pthread_mutex_destroy(&m_mutex);
    }

    bool lock(){
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    bool unlock(){
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t* get(){
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};


/* 
条件变量封装类

初始化条件变量
int pthread_cond_init(pthread_cond_t *cv, const pthread_condattr_t *cattr);
    cv 条件变量对象
    cattr 条件变量属性
    成功返回0

等待条件变量成立
int pthread_cond_wait(pthread_cond_t *cv, pthread_mutex_t *mutex);
    调用该函数时，线程总处于某个临界区，持有某个互斥锁
    释放mutex防止死锁，阻塞等待唤醒，然后再获取mutex
    成功返回0

计时等待条件变量成立
int pthread_cond_timedwait(pthread_cond_t *cv, pthread_mutex_t *mutex, struct timespec *abstime);
    成功返回0，超时返回ETIMEDOUT

唤醒一个wait的线程
int pthread_cond_signal(pthread_cond_t *cv);
    成功返回0

唤醒所有wait的线程
int pthread_cond_broadcast(pthread_cond_t *cv);
    成功返回0

*/
class condvar{
public:
    condvar(){
        if(pthread_cond_init(&m_cond, NULL) != 0){
            throw std::exception();
        }
    }

    ~condvar(){
        pthread_cond_destroy(&m_cond);
    }

    // 把调用线程放入条件变量请求队列，解锁阻塞，直到特定条件发生，唤醒后重新加锁
    // 开始加锁防止线程1没进入wait cond状态时，线程2调用cond singal导致信号丢失
    bool wait(pthread_mutex_t *m_mutex){
        return pthread_cond_wait(&m_cond, m_mutex) == 0;
    }

    bool timewait(pthread_mutex_t *m_mutex, struct timespec t){
        return pthread_cond_timedwait(&m_cond, m_mutex, &t) == 0;
    }

    bool signal(){
        return pthread_cond_signal(&m_cond) == 0;
    }

    bool broadcast(){
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    pthread_cond_t m_cond;
};

#endif