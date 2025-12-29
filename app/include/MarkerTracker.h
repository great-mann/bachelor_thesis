#ifndef __UTILMARKERTRACKER_H
#define __UTILMARKERTRACKER_H



#include <string>
#include <iostream>
#include <map>

#include <opencv2/opencv.hpp>
#include <boost/thread.hpp>

#include "Marker.h"
#include "ThresholdMethod.h"

#include "DebugHelpers.h"
#include "MultiMarkerTracker.h"
#include "Anytime.h"
#include "MarkerTrackerConfig.h"
#include "MedianFlow.h"
#include "Stopwatch.h"
#include "DetectionLegacy.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
namespace cv { class Mat; }

typedef AnytimeCallback MarkerTrackerCallback;
typedef AnytimeState MarkerTrackerState;
typedef AnytimeFunction MarkerTrackerFunction;

typedef enum{
    TIME_LIMITED = (1<<0),
    BACKGROUND   = (1<<1),
    FULL         = (1<<2),
    TIME_LIMITED_PRIOR_AND_ONE_BG_SEARCH = (1<<3),
    SKIP_PRIORSEARCH = (1<<4)
} BartTrackingOptions;

/**
 * @class MarkerTracker utilMarkerTracker.h
 *
 * Marker tracking library.
 */
class MarkerTracker : public Anytime
{
public:

    /**
     * Default Constructor.
     */
    MarkerTracker(cv::Size2f markerSize=cv::Size2f(0.05,0.05));
    /**
     * Constructor using a config (can be read from yaml file).
     */
    MarkerTracker(MarkerTrackerConfig config);
    MarkerTracker(std::string &configUrl);
    MarkerTracker(cv::Size2f markerSize, 
                             ThresholdMethod method);

    /**
     * Destructor.
     */
    virtual ~MarkerTracker();


    ThresholdMethod thresholdMethod = ThresholdMethod::v1;


    void refreshCamera(const std::string& cameraName);

    void setCameraIntrinsics(const std::string& cameraName,
                             const cv::Mat& K,
                             const cv::Mat& D);
    cv::Point2f estimateMarkerPosition(std::string camera, int id, boost::posix_time::ptime timestamp){
        return multiMarkerTracker.estimateMarkerPosition(camera, id, timestamp);
    }
    KalmanMarkerList::kalmanmarkermap_t estimateMarkerPositions(std::string camera, boost::posix_time::ptime timestamp){
        return multiMarkerTracker.estimateMarkerPositions(camera, timestamp);
    }


    // ------------------------- Process calls ------------------------------------------------------------------------

    /**
     * Finds markers in an image and stores their transformation matrices.
     * @param frame
     * @param frameTime
     * @param camera_name
     * @param tracking options
     * @param timeout
     * @param downScaleFactor
     * @param scales
     */
    boost::uuids::uuid processFrame(const cv::Mat frame, boost::posix_time::ptime frameTime, std::string camera_name, int options, unsigned int timeout, float downScaleFactor=1.0f, unsigned int scales=1);
    /**
     * Finds markers in an image and stores their transformation matrices.
     * Version with parameters from the config file (or default) for timeout, scale and downScaleFactor.
     * @param frame
     * @param frameTime
     * @param camera_name
     * @param tracking options
     */
    boost::uuids::uuid processFrame(const cv::Mat frame, boost::posix_time::ptime frameTime, std::string camera_name, int options);
#if 0

    /**
     * Non blocking call for processFrame with timeout.
     * Use registerCallback from Anytime.h to be informed about results!
     * @param frame
     * @param frameTime
     * @param timeout
     * @param downScaleFactor
     * @param scales
     * @return Thread uuid
     */
    boost::uuids::uuid  processFrame_thread(const cv::Mat frame, boost::posix_time::ptime frameTime, std::string camera, unsigned int timeout, float downScaleFactor=1.0f, unsigned int scales=1);
    /**
     * Blocking call for processFrame with timeout.
     * @param frame
     * @param frameTime
     * @param timeout
     * @param downScaleFactor
     * @param scales
     */
    void processFrame_wait(const cv::Mat frame, boost::posix_time::ptime frameTime, std::string camera, unsigned int timeout, float downScaleFactor=1.0f, unsigned int scales=1);
    /**
     * Blocking call for processFrame without timeout.
     * @param frame
     * @param frameTime
     * @param downScaleFactor
     * @param scales
     */
    void processFrame_normal(const cv::Mat frame, boost::posix_time::ptime frameTime, std::string camera, float downScaleFactor, unsigned int scales, bool prior_search);
#endif

    // ------------------------- Getter/Setter ------------------------------------------------------------------------

    /**
     * Returns the image with marker contours.
     * @return contourImage
     */
    cv::Mat getImage(std::string camera, size_t milliseconds=0);
    cv::Mat getImage(std::string camera, boost::posix_time::ptime timestamp);
    cv::Mat getOriginalImage(std::string camera, size_t milliseconds=0);
    cv::Mat getOriginalImage(std::string camera, boost::posix_time::ptime timestamp);

    /**
     * Fills the marker transformations with respect to the camera.
     */
    void getMarkerTransformations(std::map<int, cv::Mat> &markerTransformations);
    /**
     * Fills the global camera transformations
     */
    void getCameraTransformations(std::map<int, cv::Mat> &transformations);
    /**
     * Fills the best global camera transformation.
     */
    float getBestCameraTransformation(std::string &camera, cv::Mat &transformation, size_t milliseconds=0);
    float getBestCameraTransformation(std::string &camera, cv::Mat &transformation, boost::posix_time::ptime time);
    /**
     * Fills the transformations of dynamic bodies.
     */
    void getDynamicBodyTransformations(std::string &camera, std::map<std::string,cv::Mat> &transformations, size_t milliseconds=0);
    void getDynamicBodyTransformations(std::string &camera, std::map<std::string,cv::Mat> &transformations, boost::posix_time::ptime &timestamp);
    /**
     * Returns the learned transformations of single markers.
     */
    std::map<int,cv::Mat> getSingleMarkerTransformations();
    /**
     * Fills a vector of all found marker ids.
     */
    void getMarkerIds(std::vector<int> &markerIds, boost::posix_time::ptime startTime);
    /**
     * Returns all detected markers.
     * @return markers
     */
    std::map<int,Marker> getMarkers(boost::posix_time::ptime timestamp) { return multiMarkerTracker.getDetectedMarkers(timestamp); };


    /**
     * Returns the camera's field of view with respect to the image size.
     * @param imageSize
     * @return fov
     */
    float getCameraFieldOfView(std::string camera, cv::Size imageSize);

    void putMarkerSize(int markerId, cv::Size2f size);
    void putMarkerTransform(int markerId, cv::Mat matrix);

    /**
     * @deprecated InstantIOLogger will be removed.
     * @return logString
     */
    std::string getLog() {
        return InstantIOLogger::flushLogStr();
    }

    /**
     * Returns the minimum marker size in pixels.
     */
    int getMinMarkerSizeInPixels() const { return config.minMarkerSizeInPixels; };
    /**
     * Sets the minimum marker size in pixels.
     */
    void setMinMarkerSizeInPixels( int s ) { config.minMarkerSizeInPixels = s; };

    /**
     * Returns the maximum of markers to find.
     */
    int getMaxNumberOfMarkers() const { return config.maxNumberOfMarkers; };
    /**
     * Sets the maximum of markers to find.
     */
    void setMaxNumberOfMarkers( int s ) { config.maxNumberOfMarkers = s; };

    /**
     * Returns if median flow is to be used.
     */
    bool getUseMedianFlow() const { return config.useMedianFlow; };
    /**
     * Sets if median flow is to be used.
     */
    void setUseMedianFlow( bool s ) { config.useMedianFlow = s; };

    /**
     * Returns the markerTracker's config.
     * @return config
     */
    MarkerTrackerConfig getConfig() { return config; };
    /**
     * Sets the markerTracker's config.
     * @param config
     */
    void setConfig(MarkerTrackerConfig config) { this->config = config; };

    /**
     * Prints timings of the time keepers.
     * @param ostream
     */
    void printTimings( std::ostream& );

    /**
     * Merges different marker detections.
     * @param lhs
     * @param rhs
     * @return merged marker vector
     */
    std::vector<Marker> mergeMarkers( std::vector<Marker>& lhs, std::vector<Marker>& rhs );


    bool writeConfig(std::string url);
    cv::Mat get_debug_image(std::string camera);
    std::string getBodyNameByMarkerId(int id){
        return multiMarkerTracker.getBodyNameByMarkerId(id);
    }

protected:


    /**
     * Sets up the marker detector.
     */
    void setup();

    bool estimateMarker(const cv::Mat &scaled,
                                        const cv::Mat &grayscale,
                                        float roiScale,
                                         Marker &priorMarker);

    void findCandidates(const std::vector<std::vector<cv::Point> > &contours,
                              std::vector<Marker> &detectedMarkers);

    bool reduceJittering(Marker& detected, Marker& prior);

    void estimatePosition(std::string camera, Marker &marker, bool undistorted);


private:
    
    /**
     * Finds markers in an image and stores their transformation matrices.
     * @param frame
     * @param frameTime
     * @param timeout
     * @param downScaleFactor
     * @param scales
     * @param do_prior
     */
    void processFrameInternal(const cv::Mat frame, boost::posix_time::ptime frameTime, std::string camera, float downScaleFactor=1.0f, unsigned int scales=1, bool do_prior=true, bool full_search=true);
    void handleDetectedMarker(std::string camera, Marker &marker, boost::posix_time::ptime frameTime, bool undistorted);

    MarkerTrackerConfig config;
    cv::Size m_markerSize3d;

    MedianFlow m_MedianFlowTracker;
    std::map<int, cv::Size2f > specificMarkerSizes;
    std::map<int, std::vector<cv::Point3f> > specificMarkerCorners3d;
    std::map<int, cv::Mat> markerTransformations;

    cv::Point2f m_markerCorners2d[4];
    std::vector<cv::Point3f> m_markerCorners3d;

//	std::vector< std::vector<cv::Point> > m_contours;
//	std::vector<Marker> m_detectedMarkers;

//	cv::Mat m_contourImage;
    //cv::Mat m_grayscaleImage;
//	cv::Mat m_thresholdImg;
    //cv::Mat canonicalMarkerImage;
    decayMap<std::string, cv::Mat> lastFrames;

    cv::Size frameSize;
//	cv::Mat camMatrix;
//	cv::Mat distCoeff;
    Stopwatch m_stopwatch;
    DetectionLegacy detector_;
    MultiMarkerTracker multiMarkerTracker;


    boost::mutex lastFrameMutex;
    boost::mutex detectedMarkerMutex;
    boost::uuids::uuid background_search_threadid;
    mutable std::mutex result_mtx_;
    std::map<int, cv::Mat> singleMarkerTransforms_;


    typedef std::map<std::string, int> debug_image_map_index_t;
    typedef std::vector<cv::Mat> Matv;
    typedef std::map<std::string, Matv> debug_image_map_t;
    debug_image_map_t debug_images;
    debug_image_map_index_t debug_image_index;
    bool debug_image_done;
    boost::mutex debug_image_mutex;
    bool debug_image_paint_new_request;
    

    // This is a hack for the Automatica Demo. We need to throttle BART in
    // order to save some CPU cycles
    //int throttle = 0;
};


#endif // __UTILMARKERTRACKER_H
