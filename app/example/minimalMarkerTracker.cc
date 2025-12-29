#include "minimalMarkerTracker.h"

#include <boost/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/bind/bind.hpp>
#include <stdio.h>
//#include <tchar.h>
#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <string>
#include <queue>
#include <limits>

using namespace std;

// The markertracker
MarkerTracker* markerTracker;

// Mutex to avoid marker updates during output
boost::mutex my_mutex;

// Output image
cv::Mat outMat;

// Number of frames from the camera
size_t numberFrames = 0;

// Number of frames with detected markers
size_t numberDetected = 0;


/**
 * The callback where we get results from the marker tracker.
 */
void callback(MarkerTrackerState state, boost::posix_time::ptime startTime) {

    std::string cam = "default"; // This is the default camera name

    // DONE_PART is sent for every single detected marker
	if (state == DONE_PART) {
        
        std::cout << "Found marker after "
            << boost::posix_time::time_duration(boost::posix_time::microsec_clock::local_time() - startTime).total_milliseconds()
            << "ms!" << "\n";

        // Get information about the current state
		std::map<int, cv::Mat> markerTransforms; // transformations camera-marker
		markerTracker->getMarkerTransformations(markerTransforms);
		std::map<int, cv::Mat> globalTransforms; // global transformation of the camera, calculated for every single marker (if in the config)
		markerTracker->getCameraTransformations(globalTransforms);
		std::vector<int> ids; // Ids of the markers
		markerTracker->getMarkerIds(ids, startTime);
        
        std::cout << "Found: ";
        for (unsigned int i = 0; i < ids.size(); ++i) {
            std::cout << ids[i] << " ";
        }
        std::cout << "\n";
        std::string trackerLog = markerTracker->getLog();
    }

    // DONE is sent when the tracker has
    //   a) found the required number of markers specified in the config, or
    //   b) the marker tracker did process all it could and did not find any more markers
    // TIMEOUT is sent when the marker tracker was interrupted because the timeout was reached
    if (state == DONE || state == TIMEOUT) {
        // So here a frame definitely ends
        ++numberFrames;
        
        boost::mutex::scoped_lock lock(my_mutex);
        boost::this_thread::disable_interruption disableInterruption;

        if (state == DONE) {
            std::cout << "Done after "
                    << boost::posix_time::time_duration(boost::posix_time::microsec_clock::local_time() - startTime).total_milliseconds()
                    << "ms. \n";
        } else {
            std::cout << "Timeout after "
                    << boost::posix_time::time_duration(boost::posix_time::microsec_clock::local_time() - startTime).total_milliseconds()
                    << "ms. \n";
        }
        
        // Print the processing times
        markerTracker->printTimings(cout);
        
        // Get the output image with markers...
        cv::Mat res = markerTracker->getImage(cam, startTime);

        
        // Check if markers were found
        std::vector<int> ids; // Ids of the markers
        markerTracker->getMarkerIds(ids, startTime);
        if (!ids.empty()) {
            ++numberDetected;
        }
        std::cout << "--- Detected markers in " << numberDetected << " of " << numberFrames << " frames." << "\n";

        // Get the transformations of single markers specified in the config file
        std::map<int, cv::Mat> sm = markerTracker->getSingleMarkerTransformations();
        std::map<int, cv::Mat>::iterator smIt;
		LOG(debug) << "SingleMarkers: " << "\n";
        for (smIt = sm.begin(); smIt != sm.end(); ++smIt) {
            int sId = smIt->first;
            cv::Mat sMat = smIt->second;
            cv::Vec3f t;
            cv::Vec4f o;
            GeometryHelpers::decomposeMat(sMat, t, o);
			LOG(debug) << "- { id:" << sId << ", position:" << t << ",  orientation:" << o << " }" << "\n";
        }

        // Get the transformations of bodies (specific arrangement of multiple markers) specified in the config
        std::map<std::string, cv::Mat> bodies;
        markerTracker->getDynamicBodyTransformations(cam, bodies, startTime);
        std::map<std::string, cv::Mat>::iterator it;
        for (it = bodies.begin(); it != bodies.end(); it++) {
            string name = it->first;
            if (name == "") { // Special case: Unknown markers are set to an empty body
                continue;
            }
            cv::Mat translationMat = it->second.inv();
            cout << "> Got body " << name << " with mat " << translationMat << "\n";

            
            // - Some code to show the position of a found body
            cv::Vec3f t;
            GeometryHelpers::decomposeMat(translationMat, t);
            std::vector<cv::Point3f> pIn;
            pIn.push_back(t);
            std::vector<cv::Point2f> pOut;
            Camera cam = markerTracker->getConfig().cameras["default"];
            GeometryHelpers::reprojectToDistorted2D(cam.camMatrix, cam.distCoeff, pIn, pOut);
            double radius = 60;
            cv::circle(res, pOut[0], radius, cv::Scalar(255, 255, 255), 3, cv::LINE_AA);
            cv::putText(res, name, pOut[0], cv::FONT_HERSHEY_SIMPLEX, radius * 0.01, cv::Scalar(255, 255, 255));
        }

        // Show the output image
        if (res.size().height != 0) {
            cv::resize(res, outMat, cv::Size(640, 480), 0, 0, cv::INTER_LINEAR);
            //cv::imshow("Output2", res );
            //cv::waitKey(5);
        } else {
            std::cout << "No result image found." << "\n";
        }
        
        
    }
}

int main(int argc, char* argv[]) {
    
    // ------ Setup -------
    
    // We need a config file, which should be the first argument
    if (argc <= 1) {
        cerr << "Please specify the location of the config file!" << endl;
        exit(1);
    }
    
    // Try the config and start the marker tracker
	string configURL= argv[1];
    markerTracker = new MarkerTracker(configURL);
    
    // Register a callback for the results
    markerTracker->registerCallback(boost::bind(&callback,  boost::placeholders::_1,  boost::placeholders::_2));

    
    // Video capture for internal input
    cv::VideoCapture capture;
    
    // Open a window to show the detected markers in the video
    cv::namedWindow("Output", cv::WINDOW_AUTOSIZE);
    
    // Image
    cv::Mat mat;
    char c = 0;

    cout << "Opening internal camera..." << endl;
    if (!capture.open(0)) {
        cout << "...opening camera failed!";
        exit(-1);
    }

    // Alter precision of cout for floats in order to have nicer text output
    cout.setf(std::ios::fixed, std::ios::floatfield);
    std::cout.precision(4);
    
    
    // ------ Program -------

    // Loop over every new frame until the user presses x
    size_t frameNumber = 0;
    boost::posix_time::ptime startTime;
    while (capture.read(mat) && c != 'x' && c != 'X') {

        cout << "**********************************************************************" << "\n";
		cout << "  Frame " << frameNumber++ << "\n";

        // Remember the start time
        long long int startTicks = cv::getTickCount();
        startTime = boost::posix_time::microsec_clock::local_time();
        boost::posix_time::microsec_clock::local_time();
        boost::posix_time::ptime frameTime = boost::posix_time::microsec_clock::local_time();
        
        // Send the current frame to the marker tracker
        //   Here we have a time limit specified in the config or default 50ms
        markerTracker->processFrame(mat.clone(), frameTime, "default", BartTrackingOptions::TIME_LIMITED);
        
        // Alternatives: 
          // 1: Overwrite variables from the config
            //markerTracker->processFrame(mat.clone(), frameTime, "default", BartTrackingOptions::TIME_LIMITED, 50, downScaleFactor, scales);
          // 2: Search without time limit
            //markerTracker->processFrame_normal(mat.clone(), boost::posix_time::microsec_clock::local_time(), "default", BartTrackingOptions::FULL);

        // Show the output
        boost::mutex::scoped_lock lock(my_mutex);
        if (!outMat.empty()) {
            //cv::imshow("Output", outMat);
            c = cv::waitKey(1);
        }
        
        
    }

    return 0;
}

