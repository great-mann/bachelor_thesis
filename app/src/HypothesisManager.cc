/* 
 * File:   HypothesisManager.h
 * Author: prenner
 *
 * Created on March 11, 2015, 5:29 PM
 */

#include "HypothesisManager.h"

    

/**
 * Default constructor.
 */
HypothesisManager::HypothesisManager() 
{
    this->hypothesesIt = this->hypotheses.end();
    this->hypothesisMutex = boost::shared_ptr<boost::recursive_mutex>(new boost::recursive_mutex());
};


HypothesisManager::~HypothesisManager() {

}

void 
HypothesisManager::startAtomicHypothesisOperation() {
    this->hypothesisMutex->lock();
}

void 
HypothesisManager::endAtomicHypothesisOperation() {
    this->hypothesisMutex->unlock();
}

void 
HypothesisManager::resetHypothesisPointer() {
    this->hypothesisMutex->lock();
    this->hypothesesIt = this->hypotheses.end();
    this->hypothesisMutex->unlock();
}

void 
HypothesisManager::addHypotheses(std::map<std::string, std::vector<Marker> > &insertionMap) {
    this->hypothesisMutex->lock();
    for (std::map<std::string, std::vector<Marker> >::iterator b = insertionMap.begin(); b != insertionMap.end(); ++b) {
        for (std::vector<Marker>::iterator m = b->second.begin(); m != b->second.end(); ++m) {
            this->hypotheses[b->first].push_back(*m);
        }
    }
    this->hypothesisMutex->unlock();
}

void 
HypothesisManager::sortHypotheses() {
    this->hypothesisMutex->lock();
    for (std::map<std::string, std::vector<Marker> >::iterator lm = this->hypotheses.begin();
            lm != this->hypotheses.end(); ) 
    {
        if (lm->second.empty()) {
            this->hypotheses.erase(lm++);
        } else {
            std::sort(lm->second.begin(), lm->second.end());
            ++lm;
        }
    }
    this->hypothesesIt = this->hypotheses.begin();
    this->hypothesisMutex->unlock();
}

void 
HypothesisManager::removeHypotheses(int markerId, std::string bodyName) {
    this->hypothesisMutex->lock();
    for (std::vector<Marker>::iterator m = this->hypotheses[bodyName].begin();
            m != this->hypotheses[bodyName].end();) {
        if (m->id == markerId) {
            this->hypotheses[bodyName].erase(m);
        } else {
            ++m;
        }
    }
    this->hypothesisMutex->unlock();
}

void 
HypothesisManager::removeHypotheses(std::string source) {
    this->hypothesisMutex->lock();
    for (std::map<std::string, std::vector<Marker> >::iterator b = this->hypotheses.begin(); 
            b != this->hypotheses.end(); ) 
    {
        for (std::vector<Marker>::iterator m = this->hypotheses[b->first].begin();
            m != this->hypotheses[b->first].end();) {
            if (m->source == source) {
                this->hypotheses[b->first].erase(m);
            } else {
                ++m;
            }
        }
        if (b->second.empty()) {
            this->hypotheses.erase(b++);
        } else {
             ++b;
        }
    }
    this->hypothesisMutex->unlock();
}

bool 
HypothesisManager::getNextHypothesis(Marker &marker) {
    this->hypothesisMutex->lock();

    while ((this->hypothesesIt != this->hypotheses.end())
            && (this->hypothesesIt->second.empty()) )
    {
        ++this->hypothesesIt;
    }

    if (this->hypothesesIt != this->hypotheses.end()) 
    {
        LOG(debug) << "There is a marker in body '" << this->hypothesesIt->first << "' \n";
        LOG(debug) << " In this body there are " << this->hypothesesIt->second.size() << "markers ";
        marker = this->hypothesesIt->second.back();
        LOG(debug) << "  -> Marker " << marker.id << " (source = " <<marker.source << ")";

        // Delete it from the likely markers vector
        this->hypothesesIt->second.pop_back();
//            if (!this->likelyMarkerIt->second.empty()) {
//                LOG(debug) << "Likely marker " << this->cameras[camera].likelyMarkerIt->second.back().id
//                                         << " is left in body " << this->cameras[camera].likelyMarkerIt->first;
//            }else{
//                LOG(debug) << "Likely marker list is now empty"
//                                         << " for body " << this->cameras[camera].likelyMarkerIt->first;
//            }

        // Increase iterator to next body or reset it
        bool foundLikelyMarker = false;
        LOG(debug) << " Looking for next likely marker...";
        for (size_t i = 0; i < this->hypotheses.size(); ++i) {
            ++this->hypothesesIt;
            if (this->hypothesesIt == this->hypotheses.end()) {
                LOG(debug) << "  Reset to beginning. ";
                this->hypothesesIt = this->hypotheses.begin();
            }
            // Check if there are likely markers
            LOG(debug) << "  Trying body " << this->hypothesesIt->first;
            if (!this->hypothesesIt->second.empty()) {
                foundLikelyMarker = true;
                break;
            }
        }
        if (!foundLikelyMarker) {
            // If we did not find any likely marker, set iterator to end()
            this->hypothesesIt = this->hypotheses.end();
        }

        LOG(debug) << "Likely maker: " << marker.id;
        if (this->hypothesesIt != this->hypotheses.end()) {
            LOG(debug) << "   Next one: " << this->hypothesesIt->second.back().id;
        } else {
            LOG(debug) << "   That was the last one.";
        }
    } else {
        this->hypothesisMutex->unlock();
        return false;
    }

    this->hypothesisMutex->unlock();
    return true;
}
