#include "MarkerTracker.h"
#include "MultiMarkerTracker.h"
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include "Anytime.h"
#include "Logger.h"
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "DetectionLegacy.h"


#define MM_TO_M 0.001f  // drop the trailing semicolon

// 1) ctor taking markerSize
MarkerTracker::MarkerTracker(cv::Size2f markerSize, 
                             ThresholdMethod method)
: m_stopwatch(),
thresholdMethod(method),
  detector_(m_stopwatch, cv::Size(80, 80), &config)  // NO stray colon above, no braces
{
    //this->config.downScaleFactor = 1.0f;
    this->config.markerSize = markerSize;
    setup();
}

// 2) ctor taking config
MarkerTracker::MarkerTracker(MarkerTrackerConfig cfg)
: config(cfg),
  m_stopwatch(),
  detector_(m_stopwatch, config.markerSizePixels, &config)
{
    setup();
}

// 3) ctor taking config file path
MarkerTracker::MarkerTracker(std::string &configUrl)
: m_stopwatch(),
  detector_(m_stopwatch, cv::Size(80, 80), &config)
{
    size_t pos = configUrl.find_last_of("\\/");
    std::string folder = (std::string::npos == pos) ? "" : configUrl.substr(0, pos);

    cv::FileStorage fs(configUrl, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        std::cerr << "Could not open config url '" << configUrl << "'\n";
        std::exit(EXIT_FAILURE);
    } else {
        this->config.read(fs["config"], folder);
    }
    std::cout << "Config was parsed.\n";
    std::cout << config.toString() << "\n";

    setup();
}

void MarkerTracker::setCameraIntrinsics(const std::string& cameraName,
                                        const cv::Mat& K,
                                        const cv::Mat& D)
{
    auto &cam = config.cameras[cameraName];   // creates if missing
    cam.camMatrix = K.clone();
    cam.distCoeff = D.clone();
}
void MarkerTracker::refreshCamera(const std::string& cameraName)
{
    auto it = config.cameras.find(cameraName);
    if (it == config.cameras.end()) return;
    multiMarkerTracker.addCamera(it->second); 
}

void
MarkerTracker::setup() {
    std::cout << "Setup..." << "\n";
    background_search_threadid = boost::uuids::random_generator()();

    this->lastFrames.setDecayTime(config.decayTime);
    multiMarkerTracker.setDecayTime(config.decayTime);

    m_markerSize3d = cv::Size2f(config.markerSize.height*1000.0, config.markerSize.width*1000.0);

    m_markerCorners2d[0] = cv::Point2f(0,0);
    m_markerCorners2d[1] = cv::Point2f(config.markerSizePixels.width-1,0);
    m_markerCorners2d[2] = cv::Point2f(config.markerSizePixels.width-1,config.markerSizePixels.height-1);
    m_markerCorners2d[3] = cv::Point2f(0,config.markerSizePixels.height-1);


    bool centerOrigin = true;
    if (centerOrigin)
    {
        m_markerCorners3d.push_back(cv::Point3f(-m_markerSize3d.width/2.0f,-m_markerSize3d.height/2.0f,0));
        m_markerCorners3d.push_back(cv::Point3f(+m_markerSize3d.width/2.0f,-m_markerSize3d.height/2.0f,0));
        m_markerCorners3d.push_back(cv::Point3f(+m_markerSize3d.width/2.0f,+m_markerSize3d.height/2.0f,0));
        m_markerCorners3d.push_back(cv::Point3f(-m_markerSize3d.width/2.0f,+m_markerSize3d.height/2.0f,0));
    }
    else
    {
        m_markerCorners3d.push_back(cv::Point3f(0,0,0));
        m_markerCorners3d.push_back(cv::Point3f(m_markerSize3d.width,0,0));
        m_markerCorners3d.push_back(cv::Point3f(m_markerSize3d.width,m_markerSize3d.height,0));
        m_markerCorners3d.push_back(cv::Point3f(0,m_markerSize3d.height,0));
    }

    for (std::map<std::string,MarkerBody>::iterator mb = config.markerBodies.begin(); mb != config.markerBodies.end(); ++mb)
    {
        multiMarkerTracker.addMarkerBody(mb->second);
        std::map<int,MarkerTransform> markers = mb->second.markers;
        for (std::map<int,MarkerTransform>::iterator mIt = markers.begin(); mIt != markers.end(); ++mIt) {
            putMarkerSize(mIt->first, mIt->second.size);
        }
    }

    for (std::map<int,SingleMarker>::iterator sm = config.singleMarkers.begin(); sm != config.singleMarkers.end(); ++sm) {
        putMarkerSize(sm->first, cv::Size2f(sm->second.size  * 1000.0f ,sm->second.size * 1000.0f));
        multiMarkerTracker.addSingleMarker(sm->second);
    }

    for (std::map<std::string,Camera>::iterator c = config.cameras.begin(); c != config.cameras.end(); ++c)
    {
        multiMarkerTracker.addCamera(c->second);
    }

    logger_enable_debug(config.debugging == 1);
    std::cout << "Setup done." << "\n";
}


/**
 * Destructor.
 */
MarkerTracker::~MarkerTracker() {

}



void MarkerTracker::getMarkerTransformations(std::map<int, cv::Mat> &markerTransformations)
{
    detectedMarkerMutex.lock();
    std::map<int,Marker> detectedMarkers = multiMarkerTracker.getDetectedMarkers();
    for (std::map<int,Marker>::iterator m = detectedMarkers.begin(); m != detectedMarkers.end(); ++m) {
        markerTransformations[m->first] = m->second.transformation.clone();
    }
    detectedMarkerMutex.unlock();
}

std::map<int, cv::Mat> MarkerTracker::getSingleMarkerTransformations() {
    return multiMarkerTracker.getSingleMarkerTransformations();
}

void MarkerTracker::getCameraTransformations(std::map<int, cv::Mat> &transformations)
{
    detectedMarkerMutex.lock();
    std::map<int,Marker> detectedMarkers = multiMarkerTracker.getDetectedMarkers();
    for (std::map<int,Marker>::iterator m = detectedMarkers.begin(); m != detectedMarkers.end(); ++m) {
        transformations[m->first] = m->second.cameraTransformation.clone();
    }
    detectedMarkerMutex.unlock();
}

float MarkerTracker::getBestCameraTransformation(std::string &camera, cv::Mat &transformation, size_t milliseconds)
{
    cv::Mat image = getImage(camera, milliseconds);
    return multiMarkerTracker.getCameraTransformation(camera, transformation, image, milliseconds);
}

float MarkerTracker::getBestCameraTransformation(std::string &camera, cv::Mat &transformation, boost::posix_time::ptime time)
{
    cv::Mat image = getImage(camera, time);
    return multiMarkerTracker.getCameraTransformation(camera, transformation, image, time);
}

void MarkerTracker::getDynamicBodyTransformations(std::string& camera, std::map<std::string, cv::Mat>& transformations, size_t milliseconds)
{
    transformations = multiMarkerTracker.getDynamicBodyTransformations(camera, milliseconds);
}

void MarkerTracker::getDynamicBodyTransformations(std::string& camera, std::map<std::string, cv::Mat>& transformations, boost::posix_time::ptime &timestamp)
{
    transformations = multiMarkerTracker.getDynamicBodyTransformations(camera, timestamp);
}


void MarkerTracker::getMarkerIds(std::vector<int> &markerIds, boost::posix_time::ptime startTime)
{
    detectedMarkerMutex.lock();
    std::map<int,Marker> detectedMarkers = multiMarkerTracker.getDetectedMarkers(startTime); //2
    for (std::map<int,Marker>::iterator m = detectedMarkers.begin(); m != detectedMarkers.end(); ++m) {
        markerIds.push_back(m->first);
    }
    detectedMarkerMutex.unlock();
}


float MarkerTracker::getCameraFieldOfView(std::string camera, cv::Size imageSize) {
    if (config.cameras.find(camera) == config.cameras.end() || config.cameras[camera].camMatrix.empty()) {
        return 0.0f;
    }
    double fovx, fovy, focalLength, aspectRatio;
    cv::Point2d principalPoint;
    // note: aperture size has no influence on FoV -> set to 10
    cv::calibrationMatrixValues(config.cameras[camera].camMatrix, imageSize, 10, 10, fovx, fovy, focalLength, principalPoint, aspectRatio);
    return (float) (fovx <= fovy ? fovx : fovy) * (CV_PI / 180);
}


void MarkerTracker::putMarkerSize(int markerId, cv::Size2f size)
{
    specificMarkerSizes[markerId] = size;

    std::vector<cv::Point3f> corners;
    corners.push_back(cv::Point3f(-size.width/2.0f,-size.height/2.0f,0));
    corners.push_back(cv::Point3f(+size.width/2.0f,-size.height/2.0f,0));
    corners.push_back(cv::Point3f(size.width/2.0f,+size.height/2.0f,0));
    corners.push_back(cv::Point3f(-size.width/2.0f,+size.height/2.0f,0));

    specificMarkerCorners3d[markerId] = corners;

    LOG(debug) << "Added marker " << markerId << " with size " << size.height << "." << "\n";
}


void MarkerTracker::putMarkerTransform(int markerId, cv::Mat matrix) {
    this->markerTransformations[markerId] = matrix;
}



boost::uuids::uuid MarkerTracker::processFrame(const cv::Mat frame, boost::posix_time::ptime frameTime, std::string camera_name, int options,
                                               unsigned int timeout, float downScaleFactor, unsigned int scales){
    //first check that the options specify exactly ONE tracking mode:
    unsigned int mode_count = 0;
    if (options & BartTrackingOptions::TIME_LIMITED) mode_count++;
    if (options & BartTrackingOptions::BACKGROUND)   mode_count++;
    if (options & BartTrackingOptions::FULL)         mode_count++;
    if (options & TIME_LIMITED_PRIOR_AND_ONE_BG_SEARCH)  mode_count++;

    assert(mode_count == 1 /* you have to specify exactly one mode! */);

    bool do_prior_search = (options & BartTrackingOptions::SKIP_PRIORSEARCH)?false:true;
    bool do_full_search  = true;

    if (options & TIME_LIMITED_PRIOR_AND_ONE_BG_SEARCH){
        LOG(debug) << "processFrame() running in TIME_LIMITED_PRIOR_AND_ONE_BG_SEARCH mode" << "\n";

        //first step: start a prior search with a timeout and without full search:
        //AnytimeFunction f = boost::bind(&MarkerTracker::processFrameInternal, this, frame, frameTime, camera_name, downScaleFactor, scales, true, false);
        AnytimeFunction f = [this, frame, frameTime, camera_name, downScaleFactor, scales]() {
            this->processFrameInternal(frame, frameTime, camera_name, downScaleFactor, scales, true, false);
        };
        boost::uuids::uuid id = runAnytime(f, timeout);
        LOG(debug) << "processFrame(TIME_LIMITED_PRIOR_AND_ONE_BG_SEARCH) prior search done " << "\n";

        //ok, we finished or we got a timeout. in any case we want to do the full search
        //as a bg thread if there is no such thread active yet/any more:
        if (!is_running(background_search_threadid)){
            LOG(debug) << "processFrame(TIME_LIMITED_PRIOR_AND_ONE_BG_SEARCH) started new BG thread" << "\n";
           // AnytimeFunction f = boost::bind(&MarkerTracker::processFrameInternal, this, frame, frameTime, camera_name, downScaleFactor, scales, false, true);
            AnytimeFunction f = [this, frame, frameTime, camera_name, downScaleFactor, scales]() {
                this->processFrameInternal(frame, frameTime, camera_name, downScaleFactor, scales, false, true);
            };
            background_search_threadid = runAnytime(f,1000); //use a long delay here (so long that e.g. gdb or valgrind does not stop it)
        }else{
            LOG(debug) << "processFrame(TIME_LIMITED_PRIOR_AND_ONE_BG_SEARCH) BG thread still active" << "\n";
        }
    }else if (options & BartTrackingOptions::FULL){
        LOG(debug) << "processFrame() running in FULL mode (prior_search_enabled=" << (do_prior_search ? "true" : "false") << ")" << "\n";
        processFrameInternal(frame, frameTime, camera_name, downScaleFactor, scales, do_prior_search, do_full_search);
        return boost::uuids::random_generator()(); //makes no sense to return anything here as we already finished anyway
   }else{
        if (options & BartTrackingOptions::TIME_LIMITED){
            LOG(debug) << "processFrame() running in TIME_LIMITED mode (prior_search_enabled=" << (do_prior_search ? "true" : "false") 
            << ")" << "\n";
        }else{
            LOG(debug) << "processFrame() running in BACKGROUND mode (prior_search_enabled=" << (do_prior_search ? "true" : "false") << "\n";
        }

        //AnytimeFunction f = boost::bind(&MarkerTracker::processFrameInternal, this, frame, frameTime, camera_name, downScaleFactor, scales, do_prior_search, do_full_search);
        AnytimeFunction f = [this, frame, frameTime, camera_name, downScaleFactor, scales, do_prior_search, do_full_search]() {
            this->processFrameInternal(frame, frameTime, camera_name, downScaleFactor, scales, do_prior_search, do_full_search);
        };
        boost::uuids::uuid id = runAnytime(f, timeout);
        return id;
    }
}


boost::uuids::uuid MarkerTracker::processFrame(const cv::Mat frame, boost::posix_time::ptime frameTime, std::string camera_name, int options) {
    //std::cout << "downscale factor: " << this->config.downScaleFactor << ", scales: " << this->config.scales << "\n";
    return processFrame(frame, frameTime, camera_name, options, this->config.timeout, this->config.downScaleFactor, this->config.scales);
}

/**
 * Finds markers in an image and outputs their transformation matrix, with timeout
 */

void MarkerTracker::processFrameInternal(const cv::Mat frame, boost::posix_time::ptime frameTime, std::string camera, float downScaleFactor, unsigned int scales, bool prior_search, bool full_search)
{
    
    cv::Mat inputImg;
    bool undistorted = false;
    if (this->config.undistortImage) {
        // If undistortion is required, check if it is possible first
        if (!config.cameras[camera].camMatrix.empty()) {
            cv::undistort(frame, inputImg, config.cameras[camera].camMatrix, config.cameras[camera].distCoeff);
            undistorted = true;
        }
    }
    if (!undistorted) {
        inputImg = frame;
    }

    //paint debug image only for prior image
    bool paint_debug_image = ((prior_search) && (!full_search) && config.debug_images);
    cv::Mat dbgFrame;
    if (paint_debug_image) {
        dbgFrame = inputImg.clone();
    }

    lastFrameMutex.lock();
    lastFrames.insert(frameTime, camera, inputImg);
    lastFrameMutex.unlock();

    cv::Scalar color(255,0,0);
    int priors = 0;

    try {
        StopwatchGuard s( m_stopwatch, "MarkerTracker::processFrame");
        LOG(debug) << "------------------ New frame @ " << frameTime << "-------------------" << "\n";

        {
            StopwatchGuard s( m_stopwatch, "MarkerTracker::multiMarkerTracker::newFrame");
            multiMarkerTracker.newFrame(camera, frameTime);
        }
        // --- NEW BLOCK throttle global full search if we have prior search enabled only ---
        static std::map<std::string, std::size_t> s_frameCounter;
        std::size_t frameIdx = ++s_frameCounter[camera];

        bool throttledFullSearch = full_search;
        const int N = 5;  // or put this into config.globalSearchInterval

        if (prior_search && full_search) {
            // Only allow full search every Nth frame when prior tracking is active
            if (frameIdx % N != 0) {
                throttledFullSearch = false;
            }
        }
        // -----------------------------------------------------------------------


        if (paint_debug_image){
            if (debug_image_index.find(camera) == debug_image_index.end()){
                //new camera
                debug_image_index[camera] = 0;
                debug_image_done = false;
                Matv matv;
                matv.push_back(cv::Mat(0,0,CV_8UC1));
                matv.push_back(cv::Mat(0,0,CV_8UC1));
                debug_image_mutex.lock();
                debug_images.insert(std::pair<std::string, Matv>(camera, matv));
                debug_image_mutex.unlock();
            }

            //debug images:
            debug_image_mutex.lock();
            debug_images[camera][debug_image_index[camera]] = inputImg.clone();
            debug_image_mutex.unlock();

            cv::putText(debug_images[camera][debug_image_index[camera]], to_simple_string(frameTime), cv::Point2f(40,40), cv::FONT_HERSHEY_PLAIN, 1.5, cv::Scalar(200));

        }


        cv::Mat grayscale = inputImg;
        this->frameSize = cv::Size(inputImg.cols, inputImg.rows);

        // Loop as long as not interrupted and not enough markers were found.
        Marker likelyMarker;
        while (multiMarkerTracker.getDetectedMarkers(frameTime).size() < config.maxNumberOfMarkers
                && (multiMarkerTracker.getNextLikelyMarker(camera, frameTime, likelyMarker) || scales >= 0 ) ) {
            interruptionPoint(); // ### INTERRUPTION POSSIBILITY ###

            // Use prior if there are not yet estimations
            if ( prior_search && (likelyMarker.id != -1)) {

                LOG(debug) << "Prior: " << likelyMarker.id << "...\n";
                LOG(debug) << "        " <<  likelyMarker.points[0].x << ", " << likelyMarker.points[0].y << "\n";
                LOG(debug) << "        " <<  likelyMarker.points[1].x << ", " << likelyMarker.points[1].y << "\n";
                LOG(debug) << "        " <<  likelyMarker.points[2].x << ", " << likelyMarker.points[2].y << "\n";
                LOG(debug) << "        " << likelyMarker.points[3].x << ", " << likelyMarker.points[3].y << "\n";




                    ++priors;

                if (paint_debug_image){
                    cv::Scalar color(100);
                    if (likelyMarker.source == "kalman") color = cv::Scalar(255);
                    debug_image_mutex.lock();
                    std::stringstream s;
                    s << likelyMarker.source << " [" << likelyMarker.id << "]";
                    likelyMarker.drawContour(debug_images[camera][debug_image_index[camera]], color,
                                             s.str());
                    debug_image_mutex.unlock();
                }

                std::vector<Marker> detectedMarkers;
                if (detector_.findMarkersWithPrior(grayscale, grayscale, config.roiScalePrior, likelyMarker, detectedMarkers, this->thresholdMethod)) {
                    LOG(debug) << "Found marker " << detectedMarkers.front().id << " using prior..." << "\n";

                    for (size_t i = 0; i < detectedMarkers.size(); ++i) {
                        reduceJittering(detectedMarkers[i], likelyMarker);
                        handleDetectedMarker(camera, detectedMarkers[i], frameTime, undistorted);
                        if (paint_debug_image){
                            debug_image_mutex.lock();
                            std::stringstream s;
                            s << "detection [" << likelyMarker.id  << "]";
                            likelyMarker.drawCross(debug_images[camera][debug_image_index[camera]], cv::Scalar(220),
                                                     s.str());
                            debug_image_mutex.unlock();
                        }
                    }

                // Try median flow if activated
                } else if (config.useMedianFlow && !lastFrames.get(0, camera).empty()) {
                    LOG(debug) << "Median Flow..." << "\n";
                    m_MedianFlowTracker.beginTracking( lastFrames.get(0,camera), inputImg );
                    if( m_MedianFlowTracker.track( likelyMarker ) ) {
                        // We have found a previous marker and use it...
                        likelyMarker.tracked = true;
                        handleDetectedMarker(camera, likelyMarker, frameTime, undistorted);
                    }
                    m_MedianFlowTracker.endTracking();
                }


            // Use normal detection if no estimated/prior marker left
            } else if (full_search){
                //----
                LOG(debug) << "Search FULL..." << "\n";

                interruptionPoint(); // ### INTERRUPTION POSSIBILITY ###

                if (grayscale.type() != CV_8UC1) {
                    // Convert the image to grayscale and blur it
                    detector_.prepareImage(inputImg, grayscale);
                }

                // If required, scale down image first
                cv::Mat scaled;
                // Vector for found markers
                std::vector<Marker> scaledMarkers;
                // Vector of ids that were already found
                std::map<int,Marker> ignoreIds = multiMarkerTracker.getDetectedMarkers(frameTime);
                for (std::map<int,Marker>::iterator it = ignoreIds.begin(); it != ignoreIds.end(); ++it) {
                }

                if (scales >= 0 && downScaleFactor < 1.0f) {
                    // Search on different scales
                    int s = scales--;

                    interruptionPoint(); // ### INTERRUPTION POSSIBILITY ###

                    std::stringstream ss;
                    ss << "MarkerTracker::processFrame_Scale_Iteration_" << s;
                    StopwatchGuard scaleIt( m_stopwatch, ss.str());

                    float currentFactor;
                    if (s >= 0) {
                        currentFactor = pow(downScaleFactor, s);
                    } else if (s == -1) {
                        currentFactor = 2;
                        // dont scale up at the moment
                        break;
                    } else {
                        break;
                    }
                    LOG(debug) << " s: " << s << "\n";
                    LOG(debug) << " Scale: " << currentFactor << "\n";

                    cv::Size scaledSize = cv::Size(grayscale.cols * currentFactor, grayscale.rows * currentFactor);
                    if (scaledSize.height == 0 || scaledSize.width == 0) {
                        LOG(debug) << " Scaled size would be zero. Aborting!";
                        continue;
                    } else {
                        LOG(debug) << " Scaled size is " << scaledSize.width << "x" << scaledSize.height;
                    }
                    cv::resize(grayscale, scaled, scaledSize, 0, 0, cv::INTER_LINEAR);
                 /*   std::cout << "[RES] " << scaled.cols << "x" << scaled.rows 
          << "  factor=" << currentFactor 
          << "  prior=" << prior_search 
          << "  full=" << full_search << std::endl;*/

                    interruptionPoint(); // ### INTERRUPTION POSSIBILITY ###

                    if (scaled.empty() || scaled.rows != scaledSize.height || scaled.cols != scaledSize.width) {
                        std::cout << "ERROR scaled image went wrong?!\n";
                        // make sure the image was scaled correctly
                        break;
                    }
                    LOG(debug) << " " << scaled.cols << "," << scaled.rows << "\n";
                    {
                        ss.str(std::string());
                        ss.clear();
                        ss << "MarkerTracker::findMarkers_Scale_Call_" << s;
                        StopwatchGuard sg( m_stopwatch, ss.str());
                        detector_.findMarkers(scaled, grayscale, scaledMarkers, cv::Point2f(0,0), s != 0, this->thresholdMethod, currentFactor); //use otsu 5th
                        std::vector<Marker>::iterator it = scaledMarkers.begin();
                        for ( ; it != scaledMarkers.end(); ) {
                            if (ignoreIds.find(it->id) != ignoreIds.end()) {
                                it = scaledMarkers.erase(it);
                            } else {
                                ++it;
                            }
                        }
                    }

                    interruptionPoint(); // ### INTERRUPTION POSSIBILITY ###

                    if (!scaledMarkers.empty()) {
                        ss.str(std::string());
                        ss.clear();
                        ss << " Found " << scaledMarkers.size() << " markers at scale " << s << "\n";
                        LOG(debug) << ss.str() << "\n";

                        if (s == 0) {
                            // dont refine if we have the original image size
                            for (size_t i = 0; i < scaledMarkers.size(); ++i) {
                                handleDetectedMarker(camera, scaledMarkers[i], frameTime, undistorted);
                            }
                            break;
                        } else {

                            interruptionPoint(); // ### INTERRUPTION POSSIBILITY ###

                            LOG(debug) << "Refining markers" << "\n";
                            ss.str(std::string());
                            ss.clear();
                            ss << "MarkerTracker::findMarkersWithPrior_Scale_Call_" << s;
                            StopwatchGuard sg( m_stopwatch, ss.str());
                            std::vector<Marker> result_markers;
                            for (size_t i = 0; i < scaledMarkers.size(); ++i) {
                                result_markers.clear();
                                detector_.findMarkersWithPrior(scaled, grayscale, config.roiScaleRefinement, scaledMarkers[i], result_markers, this->thresholdMethod);
                                for (size_t i = 0; i < result_markers.size(); ++i) {
                                    handleDetectedMarker(camera, result_markers[i], frameTime, undistorted);
                                }
                            }
                            LOG(debug) << "After refinement there are " << result_markers.size() << " markers left" << "\n";
                        }
                    }
                }else{
                    //not in full search mode, no more likely markers found (right now)
                    //to a small sleep here.
                    //DO not exit here, the full frame search finds some markers in the bg
                    boost::this_thread::sleep(boost::posix_time::microseconds(200));
                }
            }
            likelyMarker = Marker();
        }
    } catch( boost::thread_interrupted& ex ) {

        if (!full_search) multiMarkerTracker.estimationFrameFinished(camera, frameTime);
        if (paint_debug_image) debug_image_done = true;
        inform(TIMEOUT, frameTime);
        return;
    }
    if (!full_search) multiMarkerTracker.estimationFrameFinished(camera, frameTime);
    if (paint_debug_image) debug_image_done = true;
    inform(DONE, frameTime);
}



void
MarkerTracker::handleDetectedMarker(std::string camera, Marker &marker, boost::posix_time::ptime frameTime, bool undistorted) {
    // Calculate 3D pose
    if (!config.cameras[camera].camMatrix.empty()) {
        estimatePosition(camera, marker, undistorted);
    }
    detectedMarkerMutex.lock();
    multiMarkerTracker.detectedMarker(camera, marker, frameTime, this->frameSize);
    detectedMarkerMutex.unlock();
    inform(DONE_PART, frameTime);
}



std::vector<Marker>
MarkerTracker::
mergeMarkers( std::vector<Marker>& lhs, std::vector<Marker>& rhs ) {
    for(int i = 0; i < rhs.size(); i++) {
        bool foundID = false;
        for(int j = 0; j < lhs.size(); j++) {
            if(lhs[j].id == rhs[i].id) {
                foundID = true;
                break;
            }
        }
        if(!foundID) {
            lhs.push_back(rhs[i]);
        }
    }
    return lhs;
}


bool MarkerTracker::estimateMarker(const cv::Mat &scaled,
                                         const cv::Mat &grayscale,
                                         float roiScale,
                                         Marker &priorMarker)
{

    // Cancel if out of image
    std::vector<cv::Point2f> p = priorMarker.points;
    for (size_t i = 0; i < p.size(); ++i) {
        if (p[i].x < 0 || p[i].y < 0 || p[i].x >= grayscale.cols-1 || p[i].y >= grayscale.rows-1) {
            return false;
        }
    }
    StopwatchGuard s( m_stopwatch, "MarkerTracker::estimateMarker");

    std::vector<float> xs, ys;
    for (unsigned int p = 0; p < priorMarker.points.size(); ++p) {
        xs.push_back(priorMarker.points[p].x);
        ys.push_back(priorMarker.points[p].y);
    }

    // Get scale
    float scaleX = (float)grayscale.cols / (float)scaled.cols;
    float scaleY = (float)grayscale.rows / (float)scaled.rows;
    // Calculate ROI
    float maxX = *std::max_element(xs.begin(), xs.end());
    float minX = *std::min_element(xs.begin(), xs.end());
    float maxY = *std::max_element(ys.begin(), ys.end());
    float minY = *std::min_element(ys.begin(), ys.end());
    float sizeX = (maxX - minX) * scaleX;
    float sizeY = (maxY - minY) * scaleY;
    float x = minX * scaleX - (sizeX * roiScale - sizeX) / 2;
    float y = minY * scaleY - (sizeY * roiScale - sizeY) / 2;
    // Make sure that ROI won't be out of bounds
    x = std::max(x, 0.0f);
    y = std::max(y, 0.0f);
    // Calculate roi size
    float roiSizeX = std::min(sizeX * roiScale, grayscale.cols - x); //std::min(sizeX * roiScale, scaled.cols - x);
    float roiSizeY = std::min(sizeY * roiScale, grayscale.rows - y); //std::min(sizeY * roiScale, scaled.rows - y);

    //skip stupid sizes
    if ((roiSizeX <= 1.0) || (roiSizeY <= 1.0)){
        return false; //nothing found
    }

    // Apply ROI to original image
    cv::Rect roi(x, y, roiSizeX, roiSizeY);

    // Find markers in extended ROI
    cv::Mat roiImg(grayscale, roi);
    if (roiImg.type() != CV_8UC1) {
        detector_.prepareImage(roiImg, roiImg);
    }

    // Add deviation to estimated marker points
    cv::Point2f deviation(x, y);
    for (unsigned int p = 0; p < priorMarker.points.size(); ++p) {
        priorMarker.points[p] -= deviation;
    }


    detector_.refineCorners(grayscale, priorMarker);

    {
        StopwatchGuard s( m_stopwatch, "detector_.recognizeMarker::Estimation");
        if (detector_.recognizeMarker(roiImg, priorMarker)) {
            for (unsigned int p = 0; p < priorMarker.points.size(); ++p) {
                priorMarker.points[p] += deviation;
            }
            return true;
        }
    }
    return false;
}

bool contour_comp (std::vector<cv::Point> i,std::vector<cv::Point> j) { return (i.size() > j.size()); } //References dont work on linux system


bool
MarkerTracker::reduceJittering(Marker& detected, Marker& prior) {
    // Only de-jitter when marker ids matches and the prior was actually detected
    if (detected.id != prior.id
        || prior.source != "previous")
    {
        return false;
    }

    if (detected.points.size() == prior.points.size()) {
        std::vector<cv::Vec2f> diffs;
        for (unsigned int p = 0; p < detected.points.size(); ++p) {
            diffs.push_back(detected.points[p] - prior.points[p]);
        }
        for (unsigned int p = 0; p < detected.calculatedGoodFeaturesToTrack.size(); ++p) {
            diffs.push_back(detected.calculatedGoodFeaturesToTrack[p] - prior.calculatedGoodFeaturesToTrack[p]);
        }
        cv::Mat avg;
        cv::reduce(diffs, avg, cv::REDUCE_AVG, 1);
        cv::Point2f avgDiff(avg.at<float>(0),avg.at<float>(1));
        float avgNorm = cv::norm(avg);
        bool modified = false;

        float distDetection;
        if (avgNorm < config.dejitterEpsilon) {
            for (unsigned int p = 0; p < detected.points.size(); ++p) {
                cv::Point2f est = prior.points[p] + avgDiff;
                distDetection = cv::norm(est - detected.points[p]);
                if (1.5*distDetection > avgNorm) {
                    detected.points[p] = detected.points[p]*config.dejitterDamping + prior.points[p]*(1-config.dejitterDamping);
                    modified = true;
                }
            }
            for (unsigned int p = 0; p < detected.calculatedGoodFeaturesToTrack.size(); ++p) {
                cv::Point2f est = prior.calculatedGoodFeaturesToTrack[p] + avgDiff;
                distDetection = cv::norm(est - detected.calculatedGoodFeaturesToTrack[p]);
                if (1.5*distDetection > avgNorm) {
                    detected.calculatedGoodFeaturesToTrack[p] =
                        detected.calculatedGoodFeaturesToTrack[p]*config.dejitterDamping + prior.calculatedGoodFeaturesToTrack[p]*(1-config.dejitterDamping);
                        modified = true;
                }
            }
        }
        return modified;
    }
    return false;
}


cv::Mat
MarkerTracker::getImage(std::string camera, size_t milliseconds) {
    StopwatchGuard s( m_stopwatch, "MarkerTracker::getImage");
    cv::Mat contourImage;
    cv::Mat cameraImage;
    boost::posix_time::ptime timestamp = lastFrames.get(milliseconds, camera, cameraImage);
    std::map<int, Marker> detectedMarkers;
    detectedMarkers = multiMarkerTracker.getDetectedMarkers(timestamp);
    if (detectedMarkers.empty()) {
        return cameraImage.clone(); // Return empty mat if there is no valid one yet
    }
    cv::Mat markerCornersMat(cameraImage.size(), cameraImage.type());
    markerCornersMat = cv::Scalar(0);

    detectedMarkerMutex.lock();
    for (std::map<int,Marker>::iterator m = detectedMarkers.begin(); m != detectedMarkers.end(); ++m) {
        LOG(debug) << "Drawing contour for marker " << m->first << "\n";
        m->second.drawContour(markerCornersMat, cv::Scalar(0,255,0));
    }
    if (!detectedMarkers.empty()) {
        contourImage = cameraImage + markerCornersMat;
    }
    detectedMarkerMutex.unlock();

    return contourImage.clone();
}

cv::Mat
MarkerTracker::getImage(std::string camera, boost::posix_time::ptime timestamp) {
    StopwatchGuard s( m_stopwatch, "MarkerTracker::getImage");
    cv::Mat cameraImage = lastFrames.get(timestamp, camera).clone();
    cv::Mat contourImage = cameraImage;
    if (!cameraImage.empty()) {
        std::map<int, Marker> detectedMarkers = multiMarkerTracker.getDetectedMarkers(timestamp);
        cv::Mat markerCornersMat(cameraImage.size(), cameraImage.type());
        markerCornersMat = cv::Scalar(0);

        detectedMarkerMutex.lock();
        for (std::map<int,Marker>::iterator m = detectedMarkers.begin(); m != detectedMarkers.end(); ++m) {
            LOG(debug) << "Drawing contour for marker " << m->first << "\n";
            m->second.drawContour(markerCornersMat, cv::Scalar(0,255,0));
        }
        if (!detectedMarkers.empty()) {
            contourImage = cameraImage + markerCornersMat;
        }
        detectedMarkerMutex.unlock();
    }

    return contourImage;
}

cv::Mat
MarkerTracker::getOriginalImage(std::string camera, size_t milliseconds) {
    StopwatchGuard s( m_stopwatch, "MarkerTracker::getOriginalImage");
    cv::Mat image;
    std::map<int, Marker> detectedMarkers;
    boost::posix_time::ptime timestamp = multiMarkerTracker.getDetectedMarkers(milliseconds, detectedMarkers);
    return lastFrames.get(timestamp, camera).clone();
}

cv::Mat
MarkerTracker::getOriginalImage(std::string camera, boost::posix_time::ptime timestamp) {
    StopwatchGuard s( m_stopwatch, "MarkerTracker::getOriginalImage");
    return lastFrames.get(timestamp, camera).clone();
}


void MarkerTracker::estimatePosition(std::string camera, Marker& marker, bool undistorted)
{

    boost::posix_time::ptime frameTime = boost::posix_time::microsec_clock::local_time();

    if (config.cameras[camera].camMatrix.empty()) {
        return;
    }

    cv::Mat Rvec;
    cv::Mat_<float> Tvec;
    cv::Mat raux,taux;
    cv::Mat mm3;
    cv::Mat m2;

    mm3 = cv::Mat(marker.rectifiedGoodFeaturesToTrack);
    m2 = cv::Mat(marker.calculatedGoodFeaturesToTrack);
    LOG(debug) << "Marker " << marker.id << " size:" << marker.size.width << "," << marker.size.height;
    LOG(debug) << "Calc 3D Points of marker " << marker.id << ":" << "\n" << mm3 << "\n";
    LOG(debug) << "Calc 2D Points of marker " << marker.id << ":" << "\n" << m2 << "\n";

    cv::Mat distC;
    if (!undistorted) {
        distC = config.cameras[camera].distCoeff;
    }

    // Thies: changed this to ransac. does that make sense?
    // Patrick: Ransac needed 25+ms, reverted that
    cv::solvePnP(mm3, m2, config.cameras[camera].camMatrix, distC, raux, taux);

    raux.convertTo(Rvec,CV_32F);
    taux.convertTo(Tvec ,CV_32F);
    Tvec *= MM_TO_M;             // transfrom millimeter coordinates to meter
    Tvec.at<float>(1) *= -1.0f;  // invert x-axis for InstantReality coordinate frame
    Tvec.at<float>(2) *= -1.0f;  // invert x-axis for InstantReality coordinate frame

    Rvec.at<float>(0) *= -1.0f;  // invert x-axis for InstantReality coordinate frame

    cv::Mat_<float> rotMat(3,3);

    cv::Rodrigues(Rvec, rotMat);

    cv::Mat markerMat = cv::Mat::eye(4, 4, CV_32F);
    rotMat.copyTo(markerMat(cv::Rect(0,0,3,3)));
    cv::transpose(Tvec, markerMat(cv::Rect(0,3,3,1)));
    cv::Mat relTrans = markerMat;
    marker.transformation = relTrans;

    // Set global camera transformation
    
    if (multiMarkerTracker.hasStaticTransformation(marker.id)) {
        LOG(debug) << "Calculating transformation for marker " << marker.id << "\n";
        MarkerTransform transform;
        this->multiMarkerTracker.getMarkerTransformation(marker.id, transform);
        LOG(debug) << " Marker transformation is " << relTrans << "\n";
        LOG(debug) << " Body marker transformation is " << transform.matrix << "\n";
        cv::Mat globalTrans = relTrans.inv() * transform.matrix;
        LOG(debug) << " Global transformation is " << globalTrans << "\n";
        LOG(debug) << " Inverted global transformation is " << globalTrans.inv() << "\n";
        marker.cameraTransformation = globalTrans;
        marker.hasCameraTransformation = true;
    } else if (multiMarkerTracker.getBodyNameByMarkerId(marker.id) != ""
              && !multiMarkerTracker.getBodyByMarkerId(marker.id).isStatic)
    {
        LOG(debug) << "Calculating transformation for marker " << marker.id << "\n";
        MarkerTransform transform;
        this->multiMarkerTracker.getMarkerTransformation(marker.id, transform);
        LOG(debug) << " Marker transformation is " << relTrans << "\n";
        LOG(debug) << " Body marker transformation is " << transform.matrix << "\n";
        cv::Mat globalTrans = relTrans.inv() * transform.matrix;
        LOG(debug) << " Global transformation is " << globalTrans << "\n";
        marker.cameraTransformation = globalTrans;
    } else {
        LOG(debug) << "No transformation for marker " << marker.id << "\n";
    }


}

cv::Mat MarkerTracker::get_debug_image(std::string camera){
    if (!config.debug_images){
        LOG(debug) << "WARNING: debug_images=0 in config. not returning debug image\n";
        return cv::Mat();
    }
    if (debug_image_index.find(camera) == debug_image_index.end()){
        return cv::Mat();
    }
    
    debug_image_mutex.lock();

    int index = debug_image_index[camera];
    if (debug_images[camera].size() < index){
        //not yet filled with data
        return cv::Mat();
    }



    //draw data to new buffer
    if (debug_image_done){
        debug_image_index[camera] = 1-index;
        debug_image_done = false;
    }
    //printf("RET %d\n",index);
    cv::Mat result = debug_images[camera][index].clone();


    debug_image_mutex.unlock();
    return result;
}

void
MarkerTracker
::printTimings( std::ostream& out ) {
    m_stopwatch.print(out);
    m_MedianFlowTracker.printTimings(out);

}

bool
MarkerTracker::writeConfig(std::string url) {
    cv::FileStorage fs(url, cv::FileStorage::WRITE);
    this->config.write(fs);
    fs.release();

    return true;
}
