#ifndef __BART_RSB_SERVER_H
#define __BART_RSB_SERVER_H


#include <string>
#include <queue>

#include <iostream>
#include <sstream>
#include <string>

// RSB
#include <rsb/Listener.h>
#include <rsb/Factory.h>
#include <rsb/filter/ScopeFilter.h>
#include <rsb/converter/Converter.h>
#include <rsb/converter/Repository.h>
#include <rsb/converter/ProtocolBufferConverter.h>
#include <rsb/util/EventQueuePushHandler.h>
#include <rsb/MetaData.h>
#include <rsb/EventCollections.h>
#include <rsb/converter/EventsByScopeMapConverter.h>
#include <rsb/EventQueuePushHandler.h>
#include <rsb/Event.h>


// RST
#include <rst/vision/Faces.pb.h>
#include <rst/vision/Face.pb.h>
#include <rst/math/Vec2DInt.pb.h>
#include <rst/geometry/BoundingBox.pb.h>
#include <rst/stochastics/MixtureOfGaussian1D.pb.h>
#include <rst/vision/HeadObjects.pb.h>
#include <rst/vision/HeadObject.pb.h>
#include <rst/converters/opencv/IplImageConverter.h>
#include <rst/math/MatrixDouble.pb.h>

// RSC
#include <rsc/misc/langutils.h>
#include <rsc/threading/SynchronizedQueue.h>
#include <rsc/logging/Logger.h>

// CV
#include <opencv/cv.h>

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <boost/thread.hpp>

#include <MarkerTracker.h>

class BartRSBServer
{
public:
	BartRSBServer(std::string, std::string, bool, bool);
  
private:
	typedef rsc::threading::SynchronizedQueue<rsb::EventPtr> ImageQueue;
	typedef boost::shared_ptr<ImageQueue> ImageQueuePtr;
	typedef rsb::util::EventQueuePushHandler ImageHandler;
	typedef boost::shared_ptr<ImageHandler> ImageHandlerPtr;

	ImageQueuePtr imageQueue;
	ImageHandlerPtr imageHandler;

	bool invert = false;
	bool debug = false;

	rsb::ListenerPtr listener;
	rsb::Informer<rst::math::MatrixDouble>::Ptr informer;

    boost::posix_time::ptime startTime;
	void callback(MarkerTrackerState state, boost::posix_time::ptime startTime);

	// Marker Tracker
	MarkerTracker *markerTracker;
	cv::Mat bestGlobalTransform;


};



#endif 
