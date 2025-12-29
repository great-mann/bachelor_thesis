/* 
 * File:   Anytime.h
 * Author: prenner
 *
 * Created on June 10, 2014, 5:54 PM
 */

#ifndef ANYTIME_H
#define	ANYTIME_H

#include <deque>

#include <boost/uuid/uuid.hpp>   
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>


enum AnytimeState {
    DONE_PART,
    ABORTED,
    TIMEOUT,
    DONE,
};


typedef std::function<void()> AnytimeFunction;
typedef boost::function<void(AnytimeState, boost::posix_time::ptime)> AnytimeCallback;


struct WorkerThread {
    
    WorkerThread(AnytimeFunction function, int timeout_ms) {
        this->thread = new boost::thread(function);
        this->startTime = boost::posix_time::microsec_clock::local_time();
        this->timeout_ms = boost::posix_time::milliseconds(timeout_ms);
        this->id = boost::uuids::random_generator()();
    }
    
    ~WorkerThread() {
        delete thread;
    }
    
    boost::uuids::uuid getId() {
        return this->id;
    }
    
    bool try_join() {
        return thread->timed_join(boost::posix_time::milliseconds(0));
    }
    
    void join() {
        this->thread->join();
    }
    
    void interrupt() {
        this->thread->interrupt();
    }
    
    boost::uuids::uuid id;
    boost::thread* thread;
    boost::posix_time::ptime startTime;
    boost::posix_time::time_duration timeout_ms;
};


class Anytime {
    
public:
    
    /**
     * Constructor: Setup timing.
     */
    Anytime(int maxThreads=10);
    /**
     * Destructor.
     */
    ~Anytime();
    
    
    int registerCallback(AnytimeCallback callback);
    void unregisterCallback(int callbackId);
	
    void inform(AnytimeState state, boost::posix_time::ptime startTime);
    
    
    /**
     * Informs the anytime timer that there was a job started.
     * -> Starts the timer.
     * @param timeout timeout at which processing should be stopped. Use -1 for no timeout.
     * @return a job id
     */
    boost::uuids::uuid runAnytime(AnytimeFunction function, int timeout_ms);
    
    void interrupt(boost::uuids::uuid jobId);
    void join(boost::uuids::uuid jobId);
    bool is_running(boost::uuids::uuid jobId);
protected:
    
    void TimeoutThread();
    
    inline void interruptionPoint() {
        boost::this_thread::interruption_point();
    }
    
private:
    
    std::map<int, AnytimeCallback> registeredCallbacks;
    int nextCallbackId;
    
    boost::thread* timeoutThread;
	std::map<boost::uuids::uuid, boost::shared_ptr<WorkerThread> > workerThreads;
    std::deque<boost::uuids::uuid> workerThreadIds;
    
    boost::mutex mutex;
    
    int maxThreads;
};


#endif	/* ANYTIME_H */

