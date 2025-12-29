#include "bart_ros_node.h"

#include <boost/thread.hpp>
#include <boost/program_options.hpp>

#include <stdio.h>
//#include <tchar.h>
#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <string>
#include <queue>
#include <limits>

#include <ros/ros.h>
#include <dynamic_reconfigure/server.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/CameraInfo.h>
#include <camera_info_manager/camera_info_manager.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include <geometry_msgs/Pose.h>
#include <geometry_msgs/Point.h>
#include <geometry_msgs/Quaternion.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>

#include "bart_ros/bart_rosConfig.h"

#define BART_WINDOW_NAME "bart ros tracking results (scaled)"
#include <boost/lockfree/spsc_queue.hpp>

using namespace std;

//the camera topic. DO NOT modify this, use ROS remapping instead
std::string camera_input_topic = "/bart/input";

/**
 * The marker tracker (BART).
 */
MarkerTracker* markerTracker;

/**
 * The publisher for found markers.
 */
ros::Publisher markerPublisher;

/**
 * The subscriber for found input images.
 */
ros::Subscriber image_sub;
ros::Time ros_image_time;

/**
 * The publisher for found markers.
 */
boost::mutex connect_cb_mutex_;
bool has_subscribers;
ros::NodeHandle *nh;
bool subscription_active = false;

/**
  * ros dynamic reconf config
  */
bart_ros::bart_rosConfig config;

struct trackingresult_t{
    std::map<std::string, cv::Mat> bodies;
    cv::Mat image;
};

//std::vector<trackingresult_t> ckingresult_queue;
boost::lockfree::spsc_queue<trackingresult_t> trackingresult_queue(1);

/**
 * Timing log.
 */
boost::posix_time::ptime startTime;
size_t numberFrames = 0;
size_t numberDetected = 0;


/**
 * Logging enabled?
 */
bool do_logging = false;
/**
 * How much scaling is applied per scale.
 */
float downScaleFactor = 0.5f;
/**
 * Number of scales.
 */
int scales = 2;
/**
 * Timeout in ms.
 */
int timeout_ms = 50;

/**
 * Internal variables.
 */
boost::mutex mutex;
std::deque<boost::posix_time::ptime> fullFrames;
std::map<boost::posix_time::ptime, std::string> imageFrameIds;
cv::Mat outMat;


/**
 * Returns the rotation of a matrix as quaternion.
 * @param a
 * @return
 */
geometry_msgs::Quaternion CalculateRotation(cv::Mat a) {

    geometry_msgs::Quaternion q;

    float trace = a.at<float>(0,0) + a.at<float>(1,1) + a.at<float>(2,2);
    if (trace > 0) {
        float s = 0.5 / sqrt(trace + 1.0f);
        q.w = 0.25 / s;
        q.x = (a.at<float>(2,1) - a.at<float>(1,2)) * s;
        q.y = (a.at<float>(0,2) - a.at<float>(2,0)) * s;
        q.z = (a.at<float>(1,0) - a.at<float>(0,1)) * s;
    } else {
        if (a.at<float>(0,0) > a.at<float>(1,1) && a.at<float>(0,0) > a.at<float>(2,2)) {
            float s = 2.0f * sqrt(1.0 + a.at<float>(0,0) - a.at<float>(1,1) - a.at<float>(2,2));
            q.w = (a.at<float>(2,1) - a.at<float>(1,2)) / s;
            q.x = 0.25 * s;
            q.y = (a.at<float>(0,1) + a.at<float>(1,0)) / s;
            q.z = (a.at<float>(0,2) + a.at<float>(2,0)) / s;
        } else if (a.at<float>(1,1) > a.at<float>(2,2)) {
            float s = 2.0f * sqrt(1.0 + a.at<float>(1,1) - a.at<float>(0,0) - a.at<float>(2,2));
            q.w = (a.at<float>(0,2) - a.at<float>(2,0)) / s;
            q.x = (a.at<float>(0,1) + a.at<float>(1,0)) / s;
            q.y = 0.25 * s;
            q.z = (a.at<float>(1,2) + a.at<float>(2,1)) / s;
        } else {
            float s = 2.0f * sqrt(1.0 + a.at<float>(2,2) - a.at<float>(0,0) - a.at<float>(1,1));
            q.w = (a.at<float>(1,0) - a.at<float>(0,1)) / s;
            q.x = (a.at<float>(0,2) + a.at<float>(2,0)) / s;
            q.y = (a.at<float>(1,2) + a.at<float>(2,1)) / s;
            q.z = 0.25 * s;
        }
    }

    return q;
}


/**
 * Returns the position of a matrix as Vec3f.
 * @param a
 * @return
 */
geometry_msgs::Point CalculatePosition(cv::Mat a) {

    geometry_msgs::Point p;

    p.x = a.at<float>(3,0);
    p.y = a.at<float>(3,1);
    p.z = a.at<float>(3,2);

    return p;
}


/**
 * Callback for camera Info (includes the calibration matrix url)
 */

void
cameraInfoCallback(const sensor_msgs::CameraInfo::ConstPtr& msg,
                   sensor_msgs::CameraInfoPtr& cameraInfo){
    ROS_INFO("incoming cameraInfo");
    cameraInfo = boost::make_shared<sensor_msgs::CameraInfo>();
    *cameraInfo = *msg;
}


/**
 * Callback for input images from /bart/input.
 */
void imageCallback(const sensor_msgs::ImageConstPtr& msg)
{

    cv_bridge::CvImagePtr cv_ptr;
    try
    {
      cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
    }
    catch (cv_bridge::Exception& e)
    {
      ROS_ERROR("cv_bridge Exception: %s", e.what());
      return;
    }

    // Image
    cv::Mat mat = cv_ptr->image;
    char c = 0;

    // Save timestamp.
    boost::posix_time::ptime frameTime = boost::posix_time::microsec_clock::local_time();
    cv_ptr->image;

    ros_image_time = msg->header.stamp;

    // Save the frame_id
    imageFrameIds[frameTime] =  msg->header.frame_id;

    // Start tracking.
    markerTracker->processFrame(mat.clone(), frameTime, "default", TIME_LIMITED, timeout_ms, downScaleFactor, scales);

//    if (show_markers) {
//        mutex.lock();
//        if (!outMat.empty()) {
//            cv::imshow("Output", outMat);
//            c = cv::waitKey(1);
//        }
//        mutex.unlock();
//    }

}




void enqueue_trackingresults(trackingresult_t result){
    trackingresult_queue.push(result);
}

void visualize_trackingresults(){
    trackingresult_t result;
    bool visible = false;

    while(true){
        if (config.show_results){
            if (trackingresult_queue.pop(result)){
                std::map<std::string, cv::Mat> bodies = result.bodies;
                cv::Mat res = result.image;

                std::map<std::string, cv::Mat>::iterator it;
                    for (it = bodies.begin(); it != bodies.end(); it++) {
                        string name = it->first;
                        if (name == "") {
                            continue;
                        }


                        cv::Mat translationMat = it->second.inv();
                        cv::Vec3f t;
                        GeometryHelpers::decomposeMat(translationMat, t);

                        //if (do_logging)
                        //    printf("> got body '%s'\n", name.c_str());
                        //if (do_logging)
                        //    std::cout << translationMat << "\n";

                        //cv::Point2f coord_px;
                        //float dist = GeometryHelpers::reprojectTo2D(markerTracker->getConfig().cameras[cam].camMatrix, translation.inv(), coord_px);

                        std::vector<cv::Point3f> pIn;
                        pIn.push_back(t);
                        std::vector<cv::Point2f> pOut;
                        Camera cam = markerTracker->getConfig().cameras["default"];
                        GeometryHelpers::reprojectToDistorted2D(cam.camMatrix, cam.distCoeff, pIn, pOut);

                        double radius = 60;
                        cv::circle(res, pOut[0], radius, cv::Scalar(255, 255, 255), 3, CV_AA);
                        cv::putText(res, name, pOut[0], CV_FONT_HERSHEY_SIMPLEX, radius * 0.01, cv::Scalar(255, 255, 255));
                    }

                    if (res.size().height != 0) {
                        cv::resize(res, outMat,
                                   cv::Size(res.size().width * 0.5, res.size().height * 0.5),
                                   0, 0, cv::INTER_LINEAR);

                        cv::imshow(BART_WINDOW_NAME, outMat);
                        visible = true;

                    } else {
                       //if (do_logging)
                       //     std::cout << "No result image found." << "\n";
                    }
            }
            cv::waitKey(1);
        }else{
            if (visible){
                cvDestroyWindow(BART_WINDOW_NAME);
                visible = false;
                cv::waitKey();
            }
            sleep(0.1);
        }
    }
}


/**
 * BART result callback.
 * @param state
 * @param startTime
 */
void callback(MarkerTrackerState state, boost::posix_time::ptime startTime) {
    std::string cam = "default";

    if (state == DONE || state == TIMEOUT) {

        mutex.lock();

        try {

            boost::this_thread::disable_interruption disableInterruption;

            if (state == DONE) {
                if (do_logging)
                    std::cout << "Done after "
                        << boost::posix_time::time_duration(boost::posix_time::microsec_clock::local_time() - startTime).total_milliseconds()
                        << "ms. \n";
            } else {
                if (do_logging)
                    std::cout << "Timeout after "
                        << boost::posix_time::time_duration(boost::posix_time::microsec_clock::local_time() - startTime).total_milliseconds()
                        << "ms. \n";
            }

            // Check if the marker was found
            ++numberFrames;
            std::vector<int> ids;
            markerTracker->getMarkerIds(ids, startTime);
            if (!ids.empty()) {
                ++numberDetected;
            }
            if (do_logging)
                std::cout << "--- Detected markers in " << numberDetected << " of " << numberFrames << " frames." << "\n";

            // Print the processing times
            if (do_logging)
                markerTracker->printTimings(std::cout);

            // Publish markers
            std::map<int, cv::Mat> markerTransforms;
            std::map<int, cv::Mat> globalTransforms;
            markerTracker->getMarkerTransformations(markerTransforms);
            markerTracker->getCameraTransformations(globalTransforms);
            //std::vector<int> ids;
            visualization_msgs::MarkerArray markers;
            visualization_msgs::Marker m;
            std::stringstream ss;
            ss << "found marker ids: ";
            for (unsigned int i = 0; i < ids.size(); ++i) {
                m.id = ids[i];
                m.header.frame_id = imageFrameIds[startTime];
                m.header.stamp = ros_image_time;
                imageFrameIds.erase(startTime);
                geometry_msgs::Pose pose;
                cv::Mat matrix = markerTransforms[ids[i]];
                if (matrix.cols == 4 && matrix.rows == 4) {
                    pose.position = CalculatePosition(matrix);
                    pose.orientation = CalculateRotation(matrix);
                    m.pose = pose;
                }
                markers.markers.push_back(m);
                ss << ids[i] << " ";
            }

            ROS_INFO_STREAM_THROTTLE(0.5, ss.str());

            // Publish markers
            markerPublisher.publish(markers);

            if (config.show_results){
                std::string trackerLog = markerTracker->getLog();

                // Output the image with markers...
                cv::Mat res = markerTracker->getImage(cam, startTime);

                // Write the body names
                std::map<std::string, cv::Mat> bodies;
                markerTracker->getDynamicBodyTransformations(cam, bodies, startTime);

                trackingresult_t result;
                result.bodies = bodies;
                result.image = res;
                enqueue_trackingresults(result);
             }
         } catch (...) {
            std::cout << "E >>> Something went wrong in Bart" << std::endl;
         }

        mutex.unlock();
    }
}


void update_image_subscription(){
    ROS_INFO_STREAM("markerPublisher has " << markerPublisher.getNumSubscribers()
                    << " susbcribers. showresults = " << config.show_results);
    has_subscribers = (markerPublisher.getNumSubscribers() != 0);

    bool subscription_necessary = false;

    if ((has_subscribers > 0) || (config.show_results) ){
        subscription_necessary  = true;
    }

    if (subscription_active != subscription_necessary){
        if (!subscription_necessary) {
            ROS_INFO_STREAM("unsubscribing image input topic");
            image_sub.shutdown();
        } else {
            // subscribe input
            ROS_INFO_STREAM("(re)subscribing image input topic");
            image_sub = nh->subscribe(camera_input_topic, 100, imageCallback);
        }
        subscription_active = subscription_necessary;
    }
}


void connect_cb(const ros::SingleSubscriberPublisher& pub) {
    boost::lock_guard<boost::mutex> lock(connect_cb_mutex_);
    update_image_subscription();
}

// Reconfigure Callback
void reconf_cb(const bart_ros::bart_rosConfig &config_, uint32_t level) {
    cout << "RECONF !" << std::endl;
    config = config_;

    //image subscription necessary?
    update_image_subscription();
    //config_ = config;
    /*if (genLUT && config_.whitebalance_reset) {
        NODELET_INFO_STREAM("will trigger new whitebalance LUT generation");
        genLUT = false;
        config_.whitebalance_reset = false;
    }*/
}


sensor_msgs::CameraInfoPtr fetch_camera_calibration(ros::NodeHandle *nh){
    ros::Subscriber cameraInfoSub;
    sensor_msgs::CameraInfoPtr cameraInfo;
    cameraInfoSub = nh->subscribe<sensor_msgs::CameraInfo>(ros::names::append(camera_input_topic, "camera_info"), 1,  boost::bind(cameraInfoCallback, _1, boost::ref(cameraInfo)));

    ROS_INFO("Waiting for information for camera...");

    ros::Rate r(50);
    while (ros::ok() && cameraInfo.get() == 0){
         ros::spinOnce();
         r.sleep();
    }

    cameraInfoSub.shutdown();

    if (!cameraInfo) {
         ROS_ERROR("Aborted due to missing camera information.");
         exit(EXIT_FAILURE);
    }

    return cameraInfo;
}


void update_camera_calibration(MarkerTracker *mt, sensor_msgs::CameraInfoPtr camera_info){
    MarkerTrackerConfig mcfg = mt->getConfig();
    std::map<std::string, Camera>::iterator it = mcfg.cameras.begin();

    //update all cameras for now (fixme: match cameraconfig to camera!)
    for(;it !=  mcfg.cameras.end(); it++){
       std::vector<double> cam_matrix(camera_info->K.begin(), camera_info->K.end());
       it->second.setCamMatrix(cam_matrix, 3, 3);

       std::vector<double> dcoeff_matrix(camera_info->D.begin(), camera_info->D.end());
       it->second.setDistCoeff(dcoeff_matrix, 5, 1);
    }

    //std::cout << *camera_info << std::endl;

    mt->setConfig(mcfg);
}

/**
 * Main entrance.
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char* argv[]) {

    boost::thread t(visualize_trackingresults);

    // Setup ROS
    ros::init(argc, argv, "bart");

    ros::NodeHandle n;
    nh = &n;
    ros::NodeHandle pnh("~");

    config.show_results = false;
    dynamic_reconfigure::Server<bart_ros::bart_rosConfig> srv;
    dynamic_reconfigure::Server<bart_ros::bart_rosConfig>::CallbackType f;
    f = boost::bind(&reconf_cb, _1, _2);
    srv.setCallback(f);

    {
        boost::lock_guard<boost::mutex> lock(connect_cb_mutex_);
        markerPublisher = n.advertise<visualization_msgs::MarkerArray>("bart/markers", 100, (ros::SubscriberStatusCallback)connect_cb, (ros::SubscriberStatusCallback)connect_cb);
    }

    // -- Get parameters --
    // Get downscale
    pnh.getParam("downscale_factor", downScaleFactor);
    // Get scales
    pnh.getParam("scales", scales);
    // Get timeout
    pnh.getParam("timeout_ms", timeout_ms);
    // Get config location
    string configURL = "exampleConfig.yaml";
    pnh.getParam("config_url", configURL);
    std::cerr << "> using bart config yaml '" << configURL <<"'" << std::endl;
    // Get log statistics
    pnh.getParam("log_statistics", do_logging);

    bool show_input = config.show_input;
    //bool show_markers = true;

    // Setup Marker Tracker
    if (configURL != "") {
        markerTracker = new MarkerTracker(configURL);
    } else {
        markerTracker = new MarkerTracker();
    }

    //do we want to use the camera calibration url from ros?
    bool use_ros_camera_calibration;
    pnh.getParam("use_ros_camera_calibration", use_ros_camera_calibration);
    if (use_ros_camera_calibration){
        //fetch camera info from ros
        sensor_msgs::CameraInfoPtr camera_info = fetch_camera_calibration(nh);
        //update camera calibration 
        update_camera_calibration(markerTracker, camera_info);
    }

    markerTracker->registerCallback(boost::bind(&callback, _1, _2));

    //if (show_markers)
    //    cv::namedWindow("Output", cv::WINDOW_AUTOSIZE);
    if (show_input)
        cv::namedWindow("Input", cv::WINDOW_AUTOSIZE);


    ROS_INFO("BART is set up and running!");

    ros::spin();

    return 0;
}

