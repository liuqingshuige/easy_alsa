#ifndef __FREE_MUTEX_H__
#define __FREE_MUTEX_H__
#include <pthread.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
// 互斥锁
class Mutex
{
public:
    Mutex() {pthread_mutex_init(&m_mutex, NULL);}
    virtual ~Mutex() {pthread_mutex_destroy(&m_mutex);}
    void lock() {pthread_mutex_lock(&m_mutex);}
    bool trylock() {return 0==pthread_mutex_trylock(&m_mutex);}
    void unlock() {pthread_mutex_unlock(&m_mutex);}
    pthread_mutex_t *get() { return &m_mutex; }

    void lockon() {lock();}
    void lockoff() {unlock();}

private:
    pthread_mutex_t m_mutex;
};
typedef Mutex MutexLock;

class MutexGuard
{
public:
    MutexGuard(Mutex *mutex) : m_mutex(mutex) {m_mutex->lock();}
    virtual ~MutexGuard() {m_mutex->unlock();}

private:
    Mutex *m_mutex;
};
typedef MutexGuard MutexLockGuard;

class Condition
{
public:
    Condition() {pthread_cond_init(&m_cond, NULL);}
    virtual ~Condition() {pthread_cond_destroy(&m_cond);}

public:
    void wait(Mutex *mutex) {pthread_cond_wait(&m_cond, mutex->get());}
    bool timedWait(Mutex *mutex, int ms)
    {
        struct timespec abstime;
        struct timespec now;

        clock_gettime(CLOCK_REALTIME, &now);

        unsigned long long absmsec = (now.tv_sec * 1000LL + now.tv_nsec / 1000) + ms;
        abstime.tv_sec = absmsec / 1000;
        abstime.tv_nsec = (absmsec % 1000) * 1000000;

        if (pthread_cond_timedwait(&m_cond, mutex->get(), &abstime) == 0)
          return true;
        else
          return false;
    }
    void signal(void) {pthread_cond_signal(&m_cond);}
    void broadcast(void) {pthread_cond_broadcast(&m_cond);}

private:
    pthread_cond_t m_cond;
};

#endif

