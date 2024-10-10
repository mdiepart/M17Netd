#ifndef __CONSUMERPRODUCERQUEUE_H__
#define __CONSUMERPRODUCERQUEUE_H__

#include <iostream>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

/*
 * DISCLAIMER:
 * I have no idea who wrote the following file, who to credit or
 * the license under which it is published.
 *
 * This file was slightly modified to include timeouts and fix compilation errors
 *
 * Morgan Diepart 08/2024
 */

/*
 * Some references in order
 *
 * Some code I wrote a long time before C++ 11 to do consumer producer buffers, using 2 condition variables
 * https://github.com/mdaus/coda-oss/blob/master/modules/c%2B%2B/mt/include/mt/RequestQueue.h
 *
 * A great article explaining both 2 condition variable and 1 condition variable buffers
 * https://en.wikipedia.org/wiki/Monitor_%28synchronization%29#Condition_variables
 *
 * C++ 11 thread reference:
 * http://en.cppreference.com/w/cpp/thread
 */
template<typename T>
class ConsumerProducerQueue
{
    std::condition_variable cond;
    std::mutex mutex;
    std::queue<T> cpq;
    std::string queueName ;
    unsigned int maxSize;
    std::chrono::seconds timeout;

public:

    ConsumerProducerQueue(int mxsz) : maxSize(mxsz)
    {
        timeout = std::chrono::seconds(1);
    }

    ConsumerProducerQueue(std::string qName, int mxsz) : queueName(qName), maxSize(mxsz)
    {
        timeout = std::chrono::seconds(1);
    }

    std::string& name() {
        return( queueName );
    }

    void setName( std::string qName ) {
        queueName.assign( qName );
    }

    int add(T request)
    {
        int L = 0 ;
        std::unique_lock<std::mutex> lock(mutex);
        auto res = cond.wait_for(lock, timeout, [this]() {
            return !isFull();
        });
        if(res == false)
        {
            return -1; // Timed-out
        }
        cpq.push(request);
        L = cpq.size();
        lock.unlock();
        cond.notify_all();
        return(L);
    }

    int consume(T &request)
    {
        int L = 0 ;
        std::unique_lock<std::mutex> lock(mutex);
        auto ret = cond.wait_for(lock, timeout, [this]() {
            return !isEmpty();
        });
        if(ret == false)
        {
            return -1; // Timed-out
        }
        request = cpq.front();
        cpq.pop();
        L = cpq.size();
        lock.unlock();
        cond.notify_all();
        return(L);
    }

    /**
     * Wait for the queue to contain at least one element
     *
     * @note This function waits for the queue to contain at least one element
     *          but if several threads are waiting to consume from the same queue
     *          there is no guarantee that it will still contain elements when calling consume().
     *
     * @param wait_max max duration to wait for before timing out
     *
     * @return false if timed out, true if the queue contains at least one element
     */
    int wait_for_non_empty(std::chrono::milliseconds wait_max)
    {
        std::unique_lock<std::mutex> lock(mutex);
        auto ret = cond.wait_for(lock, wait_max, [this]() {
            return !isEmpty();
        });

        return ret;
    }

    bool isFull() const
    {
        return cpq.size() >= maxSize;
    }

    bool isEmpty() const
    {
        return cpq.size() == 0;
    }

    int length() const
    {
        return cpq.size();
    }

    void clear()
    {
        std::unique_lock<std::mutex> lock(mutex);
        while (!isEmpty())
        {
            cpq.pop();
        }
        lock.unlock();
        cond.notify_all();
    }
};

#endif
