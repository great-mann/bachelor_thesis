
#include "Anytime.h"

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/bind.hpp>
#include "Logger.h"


Anytime::Anytime(int maxThreads) {
    this->nextCallbackId = 0;
    this->maxThreads = maxThreads;
    this->timeoutThread = new boost::thread(boost::bind (&Anytime::TimeoutThread, this));
}


Anytime::~Anytime() {
    
    timeoutThread->interrupt();
    timeoutThread->join();
    delete timeoutThread;
    
    for (std::map<boost::uuids::uuid, boost::shared_ptr<WorkerThread> >::iterator t = workerThreads.begin();
            t != workerThreads.end();
            ++t) {
        t->second->interrupt();
        t->second->join();
       // delete t->second;
        LOG(debug) << "Deleted worker thread " << t->first << "\n";
    }
    
}



int
Anytime::registerCallback(AnytimeCallback callback) {
//    for (size_t i = 0; i < this->registeredCallbacks.size(); ++i) {
//        if (this->registeredCallbacks[i] == callback) {
//            return; // Don't register a callback twice
//        }
//    }
    this->registeredCallbacks[nextCallbackId] = callback;
	LOG(debug) << "Registered callback " << nextCallbackId << "\n";
    
    return nextCallbackId++;
}


void 
Anytime::unregisterCallback(int callbackId) {
    this->registeredCallbacks.erase(callbackId);
	LOG(debug) << "Unregistered callback " << nextCallbackId << "\n";
}


void 
Anytime::inform(AnytimeState state, boost::posix_time::ptime startTime) {
    for (size_t i = 0; i < this->registeredCallbacks.size(); ++i) {
        registeredCallbacks[i](state, startTime); // Call Callback
    }
}


void
Anytime::TimeoutThread() {
    boost::posix_time::ptime currentTime;
    boost::posix_time::time_duration sleepTime = boost::posix_time::milliseconds(3);
    std::map<boost::uuids::uuid, boost::shared_ptr<WorkerThread> >::iterator threadIt;
    while (true) {
        boost::this_thread::sleep(sleepTime);
        
        mutex.lock();
        currentTime = boost::posix_time::microsec_clock::local_time();
        for (threadIt = this->workerThreads.begin();
                threadIt != this->workerThreads.end(); ) 
        {
            if (threadIt->second->try_join()) {
                LOG(debug) << "Worker thread [" << threadIt->first << "] ended (Runtime: "
                            << boost::posix_time::time_duration(currentTime - threadIt->second->startTime).total_milliseconds() 
							<< "ms)" << "\n";
               // delete this->workerThreads[threadIt->first];
                this->workerThreadIds.erase(
                        std::remove(this->workerThreadIds.begin(), this->workerThreadIds.end(), threadIt->first), 
                        this->workerThreadIds.end());
                this->workerThreads.erase(threadIt++);
            } else {
                if (boost::posix_time::time_duration(currentTime - threadIt->second->startTime) 
                        >= threadIt->second->timeout_ms) 
                {
					LOG(debug) << "Timeout! Interrupting worker thread " << threadIt->second->getId() << "." << "\n";
                    threadIt->second->interrupt();
                }
                ++threadIt;
            }
        }
        mutex.unlock();
    }
}

boost::uuids::uuid 
Anytime::runAnytime(AnytimeFunction function, int timeout_ms) {
    
    // Start worker thread
	LOG(debug) << "Starting worker thread..." << "\n";
    
    boost::shared_ptr<WorkerThread> worker(new WorkerThread(function, timeout_ms));
    boost::uuids::uuid workerId = worker->getId();
	LOG(debug) << "waiting for mutex " << workerId << "\n";
    mutex.lock();
    workerThreads[workerId] = worker;
    mutex.unlock();
    workerThreadIds.push_back(workerId);
	LOG(debug) << "Started worker thread " << workerId << "\n";
    
	LOG(debug) << "Currently running: " << workerThreads.size() << " worker threads" << "\n";
    
    // Terminate oldest thread if there are too many
    if (workerThreads.size() > maxThreads) {
        boost::uuids::uuid id = this->workerThreadIds.front();
		LOG(warning) << "Too many threads. Killing worker thread " << id << "\n";
        this->workerThreads[id]->interrupt();
        this->workerThreadIds.pop_front(); 
    }
    
    // Return uuid
    return worker->getId();
}

void
Anytime::interrupt(boost::uuids::uuid jobId) {
    // Interruption of worker thread automatically finished timeout thread
    if (this->workerThreads.find(jobId) != this->workerThreads.end()) {
        this->workerThreads[jobId]->interrupt();
        inform(ABORTED, this->workerThreads[jobId]->startTime);
    }
}

//check if a given uuid is still running
bool Anytime::is_running(boost::uuids::uuid jobId) {
    bool running = (this->workerThreads.find(jobId) != this->workerThreads.end());
	LOG(debug) << "thread id " << jobId << "is running ? " << (running ? "TRUE" : "FALSE") << "\n";
    return running;
}


void
Anytime::join(boost::uuids::uuid jobId) {
	LOG(debug) << "--- Waiting for thread " << jobId << "\n";

    if (this->workerThreads.find(jobId) != this->workerThreads.end()) {


		this->workerThreads[jobId]->thread->join();
        mutex.lock();
        //delete this->workerThreads[jobId];
        this->workerThreadIds.erase(
                std::remove(this->workerThreadIds.begin(), this->workerThreadIds.end(), jobId), this->workerThreadIds.end());
        this->workerThreads.erase(jobId);
        mutex.unlock();
		LOG(debug) << "--- Thread " << jobId << " ended." << "\n";
    } else {
		LOG(warning) << "--- Could not locate job " << jobId << " for joining!" << "\n";
    }
}


