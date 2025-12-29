#include "testMarkerTracker.h"
#include <filesystem>
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

namespace po = boost::program_options;
namespace fs = std::filesystem;
using namespace std;

boost::posix_time::ptime startTime;
boost::mutex my_mutex;
MarkerTracker* markerTracker;
bool do_logging = false;

std::deque<boost::posix_time::ptime> fullFrames;

cv::Mat outMat;
size_t numberFrames = 0;
size_t numberDetected = 0;

void
callback(MarkerTrackerState state, boost::posix_time::ptime startTime) {

    std::string cam = "default";

	if (state == DONE_PART) {
std::vector<int> ids;
markerTracker->getMarkerIds(ids, startTime); 
		if (do_logging)
			std::cout << "Found marker after "
			<< boost::posix_time::time_duration(boost::posix_time::microsec_clock::local_time() - startTime).total_milliseconds()
			<< "ms!" << "\n";

		std::map<int, cv::Mat> markerTransforms;
		std::map<int, cv::Mat> globalTransforms;
		markerTracker->getMarkerTransformations(markerTransforms);
		markerTracker->getCameraTransformations(globalTransforms);
//		std::vector<int> ids;
		markerTracker->getMarkerIds(ids, startTime);
		if (do_logging) {
			std::cout << "Found: ";
			for (unsigned int i = 0; i < ids.size(); ++i) {
				std::cout << ids[i] << " ";
			}
			std::cout << "\n";
		}
        std::string trackerLog = markerTracker->getLog();
    }

    if (state == DONE || state == TIMEOUT) {
        boost::mutex::scoped_lock lock(my_mutex);
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

        std::map<int, cv::Mat> sm = markerTracker->getSingleMarkerTransformations();
        std::map<int, cv::Mat>::iterator smIt;
		LOG(debug) << "SingleMarkers: " << "\n";
        for (smIt = sm.begin(); smIt != sm.end(); ++smIt) {
            int sId = smIt->first;
            cv::Mat sMat = smIt->second;
            cv::Vec3f t;
            cv::Vec4f o;
            GeometryHelpers::decomposeMat(sMat, t, o);
			LOG(debug) << "- { id:" << sId << ", size: 0.05566, position:" << t << ",  orientation:" << o << " }" << "\n";
        }


        // Output the image with markers...
        cv::Mat res = markerTracker->getImage(cam, startTime);

        // Write the body names
        std::map<std::string, cv::Mat> bodies;

        markerTracker->getDynamicBodyTransformations(cam, bodies, startTime);

        std::map<std::string, cv::Mat>::iterator it;
        for (it = bodies.begin(); it != bodies.end(); it++) {
            string name = it->first;
            if (name == "") {
                continue;
            }


            cv::Mat translationMat = it->second.inv();
            cv::Vec3f t;
            GeometryHelpers::decomposeMat(translationMat, t);

			if (do_logging)
				printf("> got body '%s'\n", name.c_str());
			if (do_logging)
				std::cout << translationMat << "\n";

            //cv::Point2f coord_px;
            //float dist = GeometryHelpers::reprojectTo2D(markerTracker->getConfig().cameras[cam].camMatrix, translation.inv(), coord_px);

            std::vector<cv::Point3f> pIn;
            pIn.push_back(t);
            std::vector<cv::Point2f> pOut;
            Camera cam = markerTracker->getConfig().cameras["default"];
            GeometryHelpers::reprojectToDistorted2D(cam.camMatrix, cam.distCoeff, pIn, pOut);

            double radius = 60;
            cv::circle(res, pOut[0], radius, cv::Scalar(255, 255, 255), 3, cv::LINE_AA);
            cv::putText(res, name, pOut[0], cv::FONT_HERSHEY_SIMPLEX, radius * 0.01, cv::Scalar(255, 255, 255));
        }

        if (res.size().height != 0) {
            cv::resize(res, outMat, cv::Size(640, 480), 0, 0, cv::INTER_LINEAR);
            //cv::imshow("Output2", res );
            //cv::waitKey(5);
  // Save the output image with timestamp
    auto now = std::chrono::system_clock::now();
    auto t_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t_c);
#else
    localtime_r(&t_c, &tm);
#endif
    std::ostringstream filename;
    filename << "./output/marker_output_"
             << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S")
             << ".png";
	if(!outMat.empty()){
    std::filesystem::create_directories("./output");
    cv::imwrite(filename.str(), outMat);
	}
	else{
		std::cout << "image empty" << "\n";
	}

	} else {
			if (do_logging)
				std::cout << "No result image found." << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << cv::getBuildInformation() << std::endl;

	string configURL="";
	string captureFilename="";
	bool captureFile = false;
	bool show_input = false;
	bool show_markers = true;
	int camera_device = 0;
	int fps = -1;

	//
	// Process the command line options
	//

	// Declare the supported options.
	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "produce help message")
		("config", po::value<string>(), "configuration file describing the camera and marker configuration (.yaml)")
		("device", po::value<int>(), "number of the camera device to be used, try alternatives 0..X if you have several cameras installed")
		("fps", po::value<int>(), "desired frame rate for playback or camera capture")
		("file", po::value<string>(), "filename of a video for offline analysis (if specified, no live camera will be used)")
		("show_input", po::value<bool>(), "set to true if the input image should be shown (default: false)")
		("show_markers", po::value<bool>(), "set to false if the image with highlighted markers should not be shown (default: true)")
		("logging", po::value<bool>(), "set to true to enable logging statistic")
		;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {	cout << desc << "\n"; return 1; }
	if (vm.count("file")) { captureFilename = vm["file"].as<string>(); captureFile = true; cout << "Reading from video file " << captureFilename << endl;  }
	if (vm.count("config")) { configURL = vm["config"].as<string>(); cout << "Reading configuration from " << configURL << endl; }
	if (vm.count("fps")) { fps = vm["fps"].as<int>(); cout << "Desired FPS: " << fps << endl; }
	if (vm.count("show_input")) { show_input = vm["show_input"].as<bool>(); }
	if (vm.count("show_markers")) { show_markers = vm["show_markers"].as<bool>(); }
	if (vm.count("logging")) { do_logging = vm["logging"].as<bool>(); }

    double downScaleFactor = 0.5f;
    int scales = 2;
    bool useLastFrameAsPrior = false;

    // Marker Tracker
    if (configURL != "") {
        markerTracker = new MarkerTracker(configURL);
    } else {
        markerTracker = new MarkerTracker();
    }
    markerTracker->registerCallback(boost::bind(&callback,  boost::placeholders::_1,  boost::placeholders::_2));

	if ( show_markers )
		cv::namedWindow("Output", cv::WINDOW_AUTOSIZE);
	if ( show_input )
		cv::namedWindow("Input", cv::WINDOW_AUTOSIZE);

	//cv::namedWindow("Threshold", cv::WINDOW_AUTOSIZE);
    // Image
    cv::Mat mat;
    char c = 0;

    // Video capture for internal input
    cv::VideoCapture capture;

    if (captureFile) {
    cout << "Trying to open file: " << captureFilename << " ..." << "/n";
	cout << "File opened succesfully" << "/n";
    // Check if the file is likely an image
    bool isImage =
        captureFilename.find(".jpg") != string::npos ||
        captureFilename.find(".png") != string::npos ||
        captureFilename.find(".jpeg") != string::npos ||
        captureFilename.find(".bmp") != string::npos;
    cout << "...isimage!" << endl; 
    if (isImage) {
        // Try loading as image
        cv::Mat image = cv::imread(captureFilename);
        cout << "...before empty!" << endl;
	if (image.empty()) {
            cout << "...loading image failed!" << endl;
            captureFile = false;
        } else {
		cout << "...notempty!" << endl;
            // Process the image once
            boost::posix_time::ptime frameTime = boost::posix_time::microsec_clock::local_time();
            markerTracker->processFrame(image.clone(), frameTime, "default", BartTrackingOptions::TIME_LIMITED);

///////////////////

//////////////////





cv::Mat annotated = markerTracker->getImage("default", frameTime);
if (!annotated.empty()) {
    //cv::imshow("Detected Markers", annotated);
    std::cout << "Press any key to exit...\n";
    cv::waitKey(0); // Wait indefinitely for a keypress
} else {
    std::cout << "No output image from tracker.\n";
}
	    cout << "...image processed !" << endl;
//////////////////////
//////////////////////
/////////////////////
std::vector<int> detectedIds;
markerTracker->getMarkerIds(detectedIds, frameTime);

if (!detectedIds.empty()) {
    std::cout << "Detected marker IDs: ";
    for (int id : detectedIds) {
        std::cout << id << " ";
    }
    std::cout << std::endl;
} else {
    std::cout << "No markers detected." << std::endl;
}



///////////////////
//
//////////////
////////////
///
            if (show_markers && !outMat.empty()) {
               // cv::imshow("Output", outMat);
                cv::waitKey(0);
            }
            return 0;
        }
    } else {
        // Try opening as video
        capture.open(captureFilename);
        if (!capture.isOpened()) {
            cout << "...opening video file failed!" << endl;
            captureFile = false;
        }
    }
}

 if (!captureFile) {
    cout << "Opening internal camera..." << endl;
    
    // Try multiple pipelines in order of preference
    const char* pipelines[] = {
        // Pipeline 1: Let libcamera handle format conversion automatically
        "libcamerasrc ! "
        "video/x-raw,width=1280,height=720 ! "
        "videoconvert ! "
        "appsink drop=true max-buffers=2 sync=false",
        
        // Pipeline 2: Accept whatever format camera gives, convert to BGR
        "libcamerasrc ! "
        "videoconvert ! "
        "video/x-raw,format=BGR,width=1280,height=720 ! "
        "appsink drop=true max-buffers=2 sync=false",
        
        // Pipeline 3: Simplest - no format specification
        "libcamerasrc ! "
        "videoconvert ! "
        "appsink drop=true max-buffers=2 sync=false"
    };
    
    cv::VideoCapture cap;
    bool opened = false;
    
    for (int i = 0; i < 3; i++) {
        std::cout << "Trying pipeline " << (i+1) << "..." << std::endl;
        cap.open(pipelines[i], cv::CAP_GSTREAMER);
        
        if (cap.isOpened()) {
            // Test if we can actually read a frame
            cv::Mat test_frame;
            if (cap.read(test_frame) && !test_frame.empty()) {
                std::cout << "Success! Camera format: " 
                          << test_frame.cols << "x" << test_frame.rows 
                          << " channels: " << test_frame.channels() << std::endl;
                opened = true;
                
                // Put the test frame back by reopening
                cap.release();
                cap.open(pipelines[i], cv::CAP_GSTREAMER);
                break;
            }
            cap.release();
        }
    }
    
    if (!opened) {
        std::cerr << "All camera pipelines failed!" << std::endl;
        std::exit(1);
    }
    
    // Assign to capture variable
    capture = cap;
}

// Add this before the main loop
startTime = boost::posix_time::microsec_clock::local_time();

// Modified main loop with fixes
size_t frameNumber = 0;
while (capture.read(mat) && c != 'x' && c != 'X') {
    if (mat.empty()) {
        std::cerr << "Warning: Empty frame received" << std::endl;
        continue;
    }
    
    // Record frame start time for FPS calculation
    boost::posix_time::ptime frameStartTime = boost::posix_time::microsec_clock::local_time();
    
    if (do_logging)
        cout << "**********************************************************************" << "\n";
    if (do_logging) 
        cout << "  Frame " << frameNumber++ << "\n";
    
    if (show_input) {
        //cv::imshow("Input", mat);
        c = cv::waitKey(5);
    }

    // Process frame with MarkerTracker
    // Note: Use the original BGR frame, not grayscale
    // Most marker trackers can handle color images
    boost::posix_time::ptime frameTime = boost::posix_time::microsec_clock::local_time();
    
    try {
        // Ensure frame is valid before processing
        if (!mat.empty() && mat.data != nullptr) {
            // Clone the frame to avoid data corruption
            cv::Mat frameClone = mat.clone();
            markerTracker->processFrame(frameClone, frameTime, "default", BartTrackingOptions::TIME_LIMITED);
        } else {
            std::cerr << "Warning: Invalid frame data, skipping..." << std::endl;
            continue;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing frame: " << e.what() << std::endl;
        continue;
    }
    
    if (show_markers) {
        boost::mutex::scoped_lock lock(my_mutex);
        if (!outMat.empty()) {
            try {
               // cv::imshow("Output", outMat);
                c = cv::waitKey(1);
            } catch (const std::exception& e) {
                std::cerr << "Error displaying output: " << e.what() << std::endl;
            }
        }
    }
    
    // Sleep for FPS control
    if (fps > 0) {
        long diff = boost::posix_time::time_duration(
            boost::posix_time::microsec_clock::local_time() - frameStartTime
        ).total_milliseconds();
        
        long sleep_time = (1000/fps) - diff;
        if (sleep_time > 0) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(sleep_time));
        }
    }
}

if (show_markers || show_input) {
    std::cout << "Press any key to exit...\n";
    cv::waitKey(0);
}

// Clean up
cv::destroyAllWindows();
delete markerTracker;

return 0;
}
