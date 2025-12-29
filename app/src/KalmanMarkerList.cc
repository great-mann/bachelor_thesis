#include "KalmanMarkerList.h"
#include <boost/date_time/posix_time/posix_time.hpp>

KalmanMarkerList::KalmanMarkerList(){
    //printf("NEW KALMANKARMERLIST %x\n", this);
    markermap_mutex = new boost::mutex;
}

KalmanMarkerList::KalmanMarkerList(const KalmanMarkerList& old){
    old.markermap_mutex->lock();
    markermap = old.markermap;
    old.markermap_mutex->unlock();
    markermap_mutex = new boost::mutex;
}

KalmanMarkerList& KalmanMarkerList::operator = (const KalmanMarkerList & old){
    old.markermap_mutex->lock();
    markermap = old.markermap;
    old.markermap_mutex->unlock();
    markermap_mutex = new boost::mutex;
	return *this;
}

KalmanMarkerList::~KalmanMarkerList(){
    delete markermap_mutex;
}


void KalmanMarkerList::update_marker(Marker m, boost::posix_time::ptime timestamp){
    int id = m.id;

    markermap_mutex->lock();
    kalmanmarkermap_t::iterator it = markermap.find(id);
    if (it == markermap.end()){
        //unknown marker, add to list!
        //printf("KALMAN item %d unknown\n",id);
        KalmanMarker kmarker(m, timestamp);
        markermap[id] = kmarker;
    }else{
        //printf("KALMAN item %d known\n",id);
        markermap[id].update_marker(m, timestamp);
    }
    markermap_mutex->unlock();
}

//cv::Point2f KalmanMarkerList::estimateMarkerPosition(int id, boost::posix_time::ptime timestamp){
//    cv::Point2f result(0,0);

//    kalmanmarkermap_t::iterator it = markermap.find(id);

//    if (it == markermap.end()){
//        //not found
//        printf("> ESTIMATE item %d unknown\n",id);
//    }else{
//        Marker(it->second).get_position(timestamp);
//        result =
//        printf("> ESTIMATE item %d OK -> %04.1f,%04.1f\n",id,result.x,result.y);
//    }
//    return result;
//}

KalmanMarkerList::kalmanmarkermap_t KalmanMarkerList::estimatedMarkerPositions(boost::posix_time::ptime timestamp){
    kalmanmarkermap_t result;

    markermap_mutex->lock();
    for(kalmanmarkermap_t::iterator it=markermap.begin(); it != markermap.end(); it++){
        KalmanMarker marker = it->second;
        if (marker.is_alive()){
            result[it->first] = marker;
        }
    }
    markermap_mutex->unlock();

    return result;
}

std::map<int,Marker> KalmanMarkerList::estimateMarkers(boost::posix_time::ptime timestamp){
    std::map<int,Marker> result;

    markermap_mutex->lock();
    for(kalmanmarkermap_t::iterator it=markermap.begin(); it != markermap.end(); it++){
        KalmanMarker kmarker = it->second;
        if (kmarker.is_alive()){
            result[it->first] = kmarker.get_marker(timestamp);
        }
    }
    markermap_mutex->unlock();

    return result;
}

void KalmanMarkerList::frameFinished(boost::posix_time::ptime timestamp){
    markermap_mutex->lock();

    //printf("KALMAN SEEN frameFinished() markerlist = ");
    /*for(kalmanmarkermap_t::iterator it=markermap.begin(); it != markermap.end(); it++){
        printf("%3d, ",(it->second).get_id());
    }
    printf(".\n");*/

    for(kalmanmarkermap_t::iterator it=markermap.begin(); it != markermap.end(); ){
        if (!(it->second).is_alive()){
            //dead marker, erase from list
            markermap.erase(it++);
        }else{
            //update frame
            (it->second).next_frame(timestamp); //update_marker(Marker(), timestamp); //this will update only for new timestamp
            it++;
        }
    }
    markermap_mutex->unlock();

}
