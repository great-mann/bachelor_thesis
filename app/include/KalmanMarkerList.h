#ifndef _KALMANMARKERLIST_H_
#define _KALMANMARKERLIST_H_

#include "KalmanMarker.h"
#include "Marker.h"
#include <boost/thread/mutex.hpp>

class KalmanMarkerList{
public:
    KalmanMarkerList();
    KalmanMarkerList(const KalmanMarkerList& old);
    KalmanMarkerList& operator = (const KalmanMarkerList & old);
    ~KalmanMarkerList();

    void update_marker(Marker marker, boost::posix_time::ptime timestamp);

    typedef std::map<int, KalmanMarker> kalmanmarkermap_t;

    kalmanmarkermap_t estimatedMarkerPositions(boost::posix_time::ptime timestamp);
//    cv::Point2f estimateMarkerPosition(int id,boost::posix_time::ptime timestamp);
    std::map<int,Marker> estimateMarkers(boost::posix_time::ptime timestamp);
    void frameFinished(boost::posix_time::ptime timestamp);
private:

    kalmanmarkermap_t markermap;
    boost::mutex *markermap_mutex;
};

#endif
