#ifndef __MULTIMARKERTRACKER_H
#define __MULTIMARKERTRACKER_H


#include <string>
#include <iostream>
#include <map>

#include <opencv2/opencv.hpp>
#include <boost/date_time.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>


#include "Marker.h"
#include "MarkerBody.h"
#include "DebugHelpers.h"
#include "GeometryHelpers.h"
#include "Camera.h"
#include "Decay.h"

#include "KalmanMarkerList.h"
#include "HypothesisManager.h"

/**
 * Camera struct for handling multiple cameras.
 */
struct MultiCamera : Camera {
    
    /**
     * Default constructor.
     */
    MultiCamera() 
    : Camera()
    {
        this->lastframeTimestamp = boost::posix_time::microsec_clock::local_time();
    };
    
    /**
     * Default constructor.
     */
    MultiCamera(Camera camera) :
    Camera(camera)
    {
        this->lastframeTimestamp = boost::posix_time::microsec_clock::local_time();
    };
    
    /**
     * Constructor for a known camera.
     * @param name
     * @param camMatrix
     * @param distCoeff
     * @param isStatic
     */
    MultiCamera(std::string name, cv::Mat camMatrix, cv::Mat distCoeff, bool isStatic, cv::Size resolution) 
    {
        this->name = name;
        this->camMatrix = camMatrix;
        this->distCoeff = distCoeff;
        this->isStatic = isStatic;
        this->resolution = resolution;
        this->lastframeTimestamp = boost::posix_time::microsec_clock::local_time();
    };
    
    ~MultiCamera() {

    }
    

    /**
     * Manages marker hypotheses for this camera.
     */
	std::map<boost::posix_time::ptime, HypothesisManager> hypothesisManager;
    
    /**
     * Timestamp of the last frame.
     */
    boost::posix_time::ptime lastframeTimestamp;
    
	/**
	* Last dynamic bodies
	*/
	std::map<std::string, cv::Mat> lastDynamicBodies;

    /*
     * Map of detected markers.
     */
    decayMap<int, Marker> detectedMarkers;
    
    /*
     * Last rvec and tvec
     */
    cv::Mat lastRVec;
    cv::Mat lastTVec;
    
};


/**
 * @class MultiMarkerTracker MultiMarkerTracker.h 
 *
 * Tracking of multiple markers.
 */
class MultiMarkerTracker
{
public:
    void estimationFrameFinished(std::string camera, boost::posix_time::ptime timestamp);
    cv::Point2f estimateMarkerPosition(std::string camera, int id, boost::posix_time::ptime timestamp);
    KalmanMarkerList::kalmanmarkermap_t estimateMarkerPositions(std::string camera, boost::posix_time::ptime timestamp);
	/**
	 * Constructor.
	 */
    MultiMarkerTracker(size_t decayTime=1000);

	/**
	 * Destructor.
	 */
    virtual ~MultiMarkerTracker() {}

    
    // ------------------------------------------------------------------------------------------
    // ----------------------- Camera and Marker body management --------------------------------
    // ------------------------------------------------------------------------------------------
    
    /**
     * Add a new known camera
     * @param camera
     */
    void addCamera(Camera &camera);
    
    /**
     * Adds a new marker body.
     * @param body
     */
	void addMarkerBody(MarkerBody &body);
    
    /**
     * Adds a new single marker ("Unknown" but known size and isStatic).
     * @param single 
     */
    void addSingleMarker(SingleMarker &single);
    
    /**
     * Set global decay time.
     * @param decayTime
     */
    void setDecayTime(size_t decayTime) {
        this->detectedMutex.lock();
        this->detectedMarkers.setDecayTime(decayTime);
        for (std::map<std::string, MultiCamera>::iterator it = this->cameras.begin(); it != this->cameras.end(); ++it) {
            it->second.detectedMarkers.setDecayTime(decayTime);
        }
        this->detectedMutex.unlock();
    }
    

    // ------------------------------------------------------------------------------------------
    // ----------------------- Methods for getting likely markers -------------------------------
    // ------------------------------------------------------------------------------------------
    
    /**
     * Signals a new frame.
     */
    void newFrame(std::string camera, boost::posix_time::ptime frameTime);
    
    
    /**
     * Returns the marker most likely to be detected.
     * @return 
     */
    bool getNextLikelyMarker(std::string camera, boost::posix_time::ptime &frameTime, Marker &marker);
    
    
    // ------------------------------------------------------------------------------------------
    // ----------------------- Methods for handling detected markers ----------------------------
    // ------------------------------------------------------------------------------------------
    
    /**
     * Notifies the multi marker tracker about a new marker detection and updates likely markers.
     * Note: This is only done if frameTimestamp is not older than lastUsedFrameTimestamp.
     * @param marker
     * @param frameTimestamp
     */
    void detectedMarker(std::string camera, 
                          Marker marker, 
                          boost::posix_time::ptime frameTimestamp, 
                          cv::Size frameSize);
    
    /**
     * Returns the detected markers from frame with timestamp.
     * @param timestamp
     * @return detected markers
     */
    std::map<int, Marker> getDetectedMarkers(boost::posix_time::ptime timestamp);
    
    /**
     * Returns the detected markers from frame before some milliseconds.
     * Use without argument for the newest result.
     * @param milliseconds
     * @return detected markers
     */
    std::map<int, Marker> getDetectedMarkers(size_t milliseconds=0);
    
    /**
     * Returns the timestamp of the detected markers from frame before some milliseconds (and the map by reference).
     * @param milliseconds
     * @return detected markers
     */
    boost::posix_time::ptime getDetectedMarkers(size_t milliseconds, std::map<int, Marker> &markerMap);
    
    
    /**
     * Calculates the most reliable transformation from the found markers.
     * @param markerMats
     * @return transformation matrix
     */
    cv::Mat calculateTransformation(std::vector<cv::Mat> &markerMats) {
		std::cerr << "calculateTransformation is not implemented yet." << "\n";
        return cv::Mat();
    }
    
    /**
     * To retrieve the most likely transformation of the camera (by reference).
     * Currently, this is the transformation calculated by the biggest found marker.
     * Use without argument for retrieving the newest transformation.
     * @return the size of the marker used for estimation (as a quality measure)
     */
    std::map<std::string, cv::Mat> getDynamicBodyTransformations(std::string &camera, size_t milliseconds=0);
    std::map<std::string, cv::Mat> getDynamicBodyTransformations(std::string &camera, boost::posix_time::ptime &timestamp);
    
    /**
     * To retrieve the most likely transformation of the camera (by reference).
     * Currently, this is the transformation calculated by the biggest found marker.
     * Use without argument for retrieving the newest transformation.
     * @return the size of the marker used for estimation (as a quality measure)
     */
    float getCameraTransformation(std::string &camera, cv::Mat &transformation, cv::Mat &image, size_t milliseconds=0);
    float getCameraTransformation(std::string &camera, cv::Mat &transformation, cv::Mat &image, boost::posix_time::ptime &timestamp);
    
    
    /**
     * Like getCameraTransformation, but only outputs camera transformations which are from a specific generation or better.
     * @param camera
     * @param transformation
     * @param timestamp
     * @param maxGeneration
     * @return 
     */
    std::pair<float,unsigned int> getConstrainedCameraTransformation(std::string &camera, 
                                                                          cv::Mat &transformation, 
                                                                          boost::posix_time::ptime &timestamp, 
                                                                          unsigned int maxGeneration);


    /**
     * Returns the transformation of marker id.
     * @param id
     * @param transform (reference for result)
     * @return success
     */
    bool getMarkerTransformation(int id, MarkerTransform& transform);
    
    
    /**
     * Returns the calculated global positions of single (unknown) markers.
     * @return marker transformations
     */
    std::map<int, cv::Mat> getSingleMarkerTransformations();
    
    // ------------------------------------------------------------------------------------------
    // ----------------------- Methods for assignment of markers to bodies ----------------------
    // ------------------------------------------------------------------------------------------
    
    /**
     * Returns the body in which the marker id is found.
     * @param id
     * @return 
     */
    MarkerBody getBodyByMarkerId(int id);
    
    /**
     * Returns the name of the body in which the marker id is found.
     * @param id
     * @return 
     */
    std::string getBodyNameByMarkerId(int id);
    
    /**
     * Returns if marker id has a static world transformation.
     * @param id
     * @return 
     */
    bool hasStaticTransformation(int id);



	/**
     * Cameras.
     */
    std::map<std::string, MultiCamera> cameras;

    
protected:
    

private:
    
    // ------------------------------------------------------------------------------------------
    // --------- Methods for estimating the global transformation of unknown markers ------------
    // ------------------------------------------------------------------------------------------
    
    float calcCombinedTransformation(std::string &camera, 
            cv::Mat &transformation,
            std::map<int, Marker> &detectedMarkers, 
            bool global,
            cv::Mat rVecPrior,
            cv::Mat tVecPrior);
    
    /**
     * Calculates the global transform for an unknown marker.
     */
    cv::Mat estimateGlobalTransform(std::string &camera, Marker &marker, boost::posix_time::ptime &timestamp);
    
    /**
     * Calculates the quality of the transformation detected for a marker.
     * @param marker
     * @return quality in [0,1]
     */
    float calcTransformationQuality(Marker &marker, std::string &camera);
    
    
    // ------------------------------------------------------------------------------------------
    // ----------------------- Methods for calculating likely markers ---------------------------
    // ------------------------------------------------------------------------------------------
    
    /**
     * Calculates the marker positions relative to one found marker.
     * @param id (of the already found marker)
     * @param position (of the already found marker)
     * @return vector of positions where other markers are likely to be found
     */
    std::vector<cv::Vec3f> calcRelativeMarkerPositions(int id, MarkerBody &markerBody, cv::Vec3f position);
    
    /**
     * Calculates the marker corners relative to one found marker.
     * @param camera Camera name
     * @param size
     * @param targetMat
     * @param originMat
     * @param transformation
     * @return vector of positions where other markers are likely to be found
     */
    std::vector<cv::Point2f>  calcRelativeMarkerCorners(std::string camera, 
                                                            cv::Mat &originMat, 
                                                            cv::Mat &targetMat, 
                                                            cv::Size2f &targetSize, 
                                                            cv::Mat &transformation);                
    std::vector<cv::Point2f>  calcRelativeMarkerCorners(std::string camera, 
                                                            cv::Mat &targetMat, 
                                                            cv::Size2f &targetSize, 
                                                            cv::Mat &transformation);                
    
    /**
     * Calculates the marker corners of all relevant markers of one body relative to one found marker.
     * @param camera Camera name
     * @param id (of the already found marker)
     * @param markerBody name of the marker body to look in
     * @param transformation of the known marker
     * @return for each marker, a vector of positions where other markers are likely to be found
     */
    std::map<int, std::vector<cv::Point2f> >  calcAllRelativeMarkerCorners(std::string camera, 
                                                                              int id, 
                                                                              MarkerBody &markerBody, 
                                                                              cv::Mat &transformation);
    
    /**
     * Calculates the marker corners of all relevant markers relative to one found marker.
     * @param camera Camera name
     * @param id (of the already found marker)
     * @param transformation of the known marker
     * @return for each marker, a vector of positions where other markers are likely to be found
     */
    std::map<int, std::vector<cv::Point2f> >  calcAllRelativeMarkerCorners(std::string camera, 
                                                                              int id,  
                                                                              cv::Mat &transformation);
    std::map<int, std::vector<cv::Point2f> >  calcAllMarkerCorners(std::string camera,  
                                                                              cv::Mat &transformation);
    
    /**
     * Calculates the next markers likely to be found with knownledge of marker marker.
     * @param camera name of the camera
     * @param marker the found marker
     * @param frameSize Resolution of the image to cut off unseeable markers
     */
    void calcLikelyMarkers(std::string camera, boost::posix_time::ptime &frameTime, Marker& marker, cv::Size& frameSize);
    void calcLikelyMarkers(std::string camera, boost::posix_time::ptime &frameTime, cv::Mat& cameraTransformation, cv::Size& frameSize);
    
    
    
    // ------------------------------------------------------------------------------------------
    // ----------------------- Variables --------------------------------------------------------
    // ------------------------------------------------------------------------------------------
    
    /**
     * Body of global static marker transformations.
     */
    std::map<std::string, MarkerBody> bodies;
    /**
     * Map marker ids -> body
     */
    std::map<int, std::string> idBodyMap;
    
    /**
     * Map of single markers ("unknown" but known size and isStatic).
     */
    std::map<int, SingleMarker> singleMarkers;
    
    /**
     * Map of unknown markers with estimated global transformation
     */
    std::map<int, cv::Mat> estimatedMarkerTransformations;
    std::map<int, size_t> estimatedMarkerIterations;

    /**
     * Map counting the number of used unknown markers for transformation calculation.
     */
    std::map<int, unsigned int> markerGenerations;
    

    
    /**
     * Currently detected markers.
     */
    decayMap<int, Marker> detectedMarkers;
    
    /**
     * Qualities of detected markers for each camera.
     */
    std::map<std::string, std::map<int, float> > markerQualities;
    
    /**
     * Decay time in ms.
     */
    size_t decayTime;
    
    /**
     * Mutex for lists.
     */
    boost::mutex detectedMutex;
    

    typedef std::map<std::string, KalmanMarkerList> kalmanMarkerlistMap_t;
    kalmanMarkerlistMap_t kalmanMarkerListMap;
    //std::map<std::string, KalmanMarkerList::iterator> kalmanMarkerListMapIt;


};


#endif // __MULTIMARKERTRACKER_H
