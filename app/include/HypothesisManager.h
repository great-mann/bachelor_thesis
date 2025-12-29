/* 
 * File:   HypothesisManager.h
 * Author: prenner
 *
 * Created on March 11, 2015, 5:29 PM
 */

#pragma once

#include "Logger.h"
#include "Marker.h"

#include <boost/thread.hpp>

#include <string>
#include <vector>
#include <map>

class HypothesisManager {
    
public:

    /**
     * Default constructor.
     */
    HypothesisManager();
    
    ~HypothesisManager();
    
    
    void startAtomicHypothesisOperation();
    
    void endAtomicHypothesisOperation();
    
    void resetHypothesisPointer();
    
    void addHypotheses(std::map<std::string, std::vector<Marker> > &insertionMap);
    
    void sortHypotheses();
    
    void removeHypotheses(int markerId, std::string bodyName);
    
    void removeHypotheses(std::string source);
    
    bool getNextHypothesis(Marker &marker);


    
    boost::shared_ptr<boost::recursive_mutex> hypothesisMutex;
    
    private:
    /**
     * Map of markers likely to be found with this camera.
     */
    std::map<std::string, std::vector<Marker> > hypotheses;
    /**
     * Iterator for the map.
     */
    std::map<std::string, std::vector<Marker> >::iterator hypothesesIt;

};