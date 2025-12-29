/*
 * File:   Decay.h
 * Author: prenner
 *
 * Created on August 5, 2014, 11:52 AM
 */

#ifndef DECAY_H
#define	DECAY_H


#include <map>
#include <deque>
#include <boost/date_time.hpp>
#include "Logger.h"
#define USE_MUTEX 1

#if !USE_MUTEX
#define MUTEX_LOCK() {}
#define MUTEX_UNLOCK() {}
#else
#define MUTEX_LOCK() internalMap_mutex->lock()
#define MUTEX_UNLOCK() internalMap_mutex->unlock()
#endif

/**
 * The decayMap class acts like a map of a timestamp and an arbitrary data type.
 * Whenever a new element is added, elements older than decay_ms are forgotten.
 */
template<typename KeyT, class DataT>
class decayMap {

    public:

        decayMap(size_t decay_ms=1000) :
            decay_ms(decay_ms)
        {
#if USE_MUTEX
            internalMap_mutex = new boost::mutex;
#endif
        }
#if USE_MUTEX
        decayMap(const decayMap& old){
            old.MUTEX_LOCK();
            int size_old = old.internalMap.size();
            internalMap = old.internalMap;
            int size_new = internalMap.size();
            assert(size_new == size_old);

            old.MUTEX_UNLOCK();
            decay_ms = old.decay_ms;
            internalMap_mutex = new boost::mutex;
        }

        decayMap& operator = (const decayMap & old){
            old.MUTEX_LOCK();
            int size_old = old.internalMap.size();
            internalMap = old.internalMap;
            int size_new = internalMap.size();
            assert(size_new == size_old);


            old.MUTEX_UNLOCK();
            decay_ms = old.decay_ms;
            internalMap_mutex = new boost::mutex;

			return *this;
        }

        ~decayMap(){
            delete internalMap_mutex;
        }
#endif
        void setDecayTime(size_t decay_ms) {
            this->decay_ms = decay_ms;
        }

        void insert(boost::posix_time::ptime timestamp, KeyT key, DataT data) {
            //track the systems time
            last_timestamp = timestamp;

            //std::cout << "DBG internalMap[] insert at timestamp "<< timestamp << "\n";
            MUTEX_LOCK();
            this->internalMap[timestamp][key] = data;
            MUTEX_UNLOCK();

            clean();
        }

        /**
         * Returns the map from timestamp timestamp.
         * @param timestamp
         * @return map
         */
        std::map<KeyT, DataT> get(boost::posix_time::ptime timestamp) {
            if (timestamp >= last_timestamp){
                //track the systems time
                last_timestamp = timestamp;
            }

            std::map<KeyT, DataT> result;
            //std::cout << "DBG "<< this << "internalMap[" << timestamp << " available = " << ((internalMap.find(timestamp)==internalMap.end())?"NOT FOUND":"FOUND") << "\n";
            MUTEX_LOCK();
            //if (internalMap.find(timestamp)!=internalMap.end()){
                result = internalMap[timestamp];
            //}

            /*typename std::map<boost::posix_time::ptime, std::map<KeyT, DataT> >::iterator it;
            for(it=internalMap.begin(); it != internalMap.end(); it++){
                std::cout << "DBG: "<< this << "internalMap["<< it->first << "] = {";
                for(typename std::map<KeyT, DataT>::iterator it2=(it->second).begin(); it2!=(it->second).end(); it2++){
                    std::cout << it2->first << ", ";
                }
                std::cout << "}\n";
            }*/

            MUTEX_UNLOCK();
            return result;
        }

        /**
         * Returns the time of the best fitting map after some milliseconds (and the map by reference).
         * @param milliseconds
         * @return map
         */
        boost::posix_time::ptime get(size_t milliseconds, std::map<KeyT, DataT> &theMap) {
            MUTEX_LOCK();
            if (this->internalMap.empty()) {
                MUTEX_UNLOCK();
                return boost::posix_time::ptime();
            }

            // Get the reverse iterator: Start search with newest timestamp (in favor for get(0))
            typename std::map<boost::posix_time::ptime, std::map<KeyT, DataT> >::reverse_iterator it =
                    this->internalMap.rbegin();
            boost::posix_time::ptime curTime = last_timestamp;
            boost::posix_time::ptime fittingTime = it->first;
            size_t fittingDuration = std::abs((int)(boost::posix_time::time_duration(curTime - it->first).total_milliseconds()));
            size_t currentDuration;
            while (++it != this->internalMap.rend()) {
                currentDuration = std::abs((int)(boost::posix_time::time_duration(curTime - it->first).total_milliseconds()));
                if (std::abs((int)(milliseconds - currentDuration)) < std::abs((int)(milliseconds - fittingDuration))) {
                    fittingDuration = currentDuration;
                    fittingTime = it->first;
                } else {
                    break; // We can stop as soon as the duration gets longer (because the map is ordered)
                }
            }

            theMap = this->internalMap[fittingTime];
            MUTEX_UNLOCK();
            return fittingTime;
        }

        /**
         * Returns the map after some milliseconds.
         * @param milliseconds
         * @return map
         */
        std::map<KeyT, DataT> get(size_t milliseconds) {
            std::map<KeyT, DataT> theMap;
            get(milliseconds, theMap);
            return theMap;
        }


        /**
         * Returns the map after some milliseconds.
         * @param milliseconds
         * @return map
         */
        DataT get(size_t milliseconds, KeyT key) {            
            std::map<KeyT, DataT> tmpMap = get(milliseconds);
            if (tmpMap.find(key) == tmpMap.end()) {
                return DataT();
            }
            return tmpMap[key];
        }

        DataT get(boost::posix_time::ptime timestamp, KeyT key) {
            if (timestamp >= last_timestamp){
                //track the systems time
                last_timestamp = timestamp;
            }

            MUTEX_LOCK();
            if (internalMap.find(timestamp) != internalMap.end()
                    && internalMap[timestamp].find(key) != internalMap[timestamp].end()) {
                DataT result =  internalMap[timestamp][key];
                MUTEX_UNLOCK();
                return result;
            }
            MUTEX_UNLOCK();
            return DataT();
        }

         boost::posix_time::ptime get(size_t milliseconds, KeyT key, DataT &data) {
            std::map<KeyT, DataT> tmpMap;
            boost::posix_time::ptime timestamp = get(milliseconds, tmpMap); //TODO: Make sure to find the right key
            if (tmpMap.find(key) != tmpMap.end()) {
                data = tmpMap[key];
                return timestamp;
            }
            return boost::posix_time::ptime();
        }


        bool empty() {
            MUTEX_LOCK();
            bool result = this->internalMap.empty();
            MUTEX_UNLOCK();
            return result;
        }

        bool empty(boost::posix_time::ptime timestamp) {
            if (timestamp >= last_timestamp){
                //track the systems time
                last_timestamp = timestamp;
            }

            MUTEX_LOCK();
            bool result = (this->internalMap.find(timestamp) != this->internalMap.end())
                    && (this->internalMap[timestamp].empty());
            MUTEX_UNLOCK();
            return result;
        }



        void clean() {
            boost::posix_time::ptime curTime = last_timestamp; //boost::posix_time::microsec_clock::local_time();
            LOG(debug) << "DBG time now = " << curTime << "\n";
            MUTEX_LOCK();
            typename std::map<boost::posix_time::ptime, std::map<KeyT, DataT> >::iterator it = this->internalMap.begin();
            // Keys are ordered by less operator, so we only need to iterate the first elements
            while (it != this->internalMap.end()
                    && boost::posix_time::time_duration(curTime - it->first).total_milliseconds() >= decay_ms) {
				LOG(debug) << "DBG Erased decayMap element: " << it->first << "\n";
                LOG(debug) << "DBG because of duration "
                        << boost::posix_time::time_duration(curTime - it->first).total_milliseconds()
						<< "\n";
                internalMap.erase(it++);
            }
            MUTEX_UNLOCK();
			LOG(debug) << "DBG Remaining elements: " << internalMap.size() << "\n";
        }

    private:
        std::map<boost::posix_time::ptime, std::map<KeyT, DataT> > internalMap;
        boost::posix_time::ptime last_timestamp;

        size_t decay_ms;
#if USE_MUTEX
        boost::mutex *internalMap_mutex;
#endif

};



#endif	/* DECAY_H */

