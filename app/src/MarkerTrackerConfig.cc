#include <opencv2/core/core.hpp>

#include "MarkerTrackerConfig.h"
#include "Camera.h"



MarkerTrackerConfig::MarkerTrackerConfig() {
    debugging = false;
    debug_images = false;
    timeout = 70;
    scales = 2;
    downScaleFactor = 0.5f;
    useMedianFlow = false;
    minContourLengthAllowed = 10.0f;
    undistortImage = false;
    decayTime = 2000;
    roiScalePrior = 1.3f;
    roiScaleRefinement = 1.3f;
    minMarkerSizeInPixels = 8;
    maxNumberOfMarkers = 10;
    markerSizePixels = cv::Size(100,100);
    markerSize = cv::Size2f(0.05,0.05);
    dejitterEpsilon = 5.0f;
    dejitterDamping = 0.05f;
}


void
MarkerTrackerConfig::write(cv::FileStorage& fs) const //Write serialization for this class
{
    fs << "{" << "debugging" << debugging
       << "debug_images" << debug_images
            << "decayTime" << (int)decayTime
            << "timeout" << (int)timeout
            << "scales" << (int)scales
            << "downScaleFactor" << downScaleFactor
            << "undistortImage" << undistortImage
            << "useMedianFlow" << useMedianFlow
            << "roiScalePrior" << roiScalePrior
            << "roiScaleRefinement" << roiScaleRefinement
            << "minMarkerSizeInPixels" << minMarkerSizeInPixels
            << "maxNumberOfMarkers" << maxNumberOfMarkers
            << "maxThreads" << maxThreads
            << "markerSizePixels" << markerSizePixels
            << "markerSize" << markerSize
            << "useMedianFlow" << useMedianFlow
            << "dejitterEpsilon" << dejitterEpsilon
            << "dejitterDamping" << dejitterDamping
            << "markerBodies" << "[";
    for (std::map<std::string, MarkerBody>::const_iterator mb = markerBodies.begin(); mb != markerBodies.end(); ++mb) {
        fs << mb->second;
    }
    fs << "]";

    fs << "}";
}

void
MarkerTrackerConfig::read(const cv::FileNode& node) {
    read(node, "");
}

void
MarkerTrackerConfig::read(const cv::FileNode& node, std::string workingDir) //Read serialization for this class
{
    debugging = (bool)(int)node["debugging"];
    debug_images = (bool)(int)node["debug_images"];
    timeout = (int)node["timeout"];
    if (timeout <= 0) {
        timeout = 50;
        std::cerr << "Timout <= 0ms: Setting to 50ms instead!" << std::endl;
    }
    scales = (int)node["scales"];
    downScaleFactor = (float)node["downScaleFactor"];
    decayTime = (size_t)(int)node["decayTime"];
    if (decayTime <= 0) {
        decayTime = 2000;
        std::cerr << "Decay time <= 0: Setting to 2000!" << std::endl;
    }
    undistortImage = (bool)(int)node["undistortImage"];
    useMedianFlow = (bool)(int)node["useMedianFlow"];
    minContourLengthAllowed = (float)node["minContourLengthAllowed"];
    roiScalePrior = (float)node["roiScalePrior"];
    roiScaleRefinement = (float)node["roiScaleRefinement"];
    minMarkerSizeInPixels = (int)node["minMarkerSizeInPixels"];
    maxNumberOfMarkers = (int)node["maxNumberOfMarkers"];
    maxThreads = (int)node["maxThreads"];
    throttle = (int)node["throttle"];
    dejitterEpsilon = (float)node["dejitterEpsilon"];
    dejitterDamping = (float)node["dejitterDamping"];
    std::vector<int> markerSizePixelsVec;
    std::vector<float> markerSizeVec;
    node["markerSizePixels"] >> markerSizePixelsVec;
    node["markerSize"] >> markerSizeVec;
    markerSizePixels = cv::Size(markerSizePixelsVec[0],markerSizePixelsVec[1]);
    markerSize = cv::Size2f(markerSizeVec[0],markerSizeVec[1]);

    cv::FileNode singles = node["singleMarkers"];
    cv::FileNodeIterator it = singles.begin(), it_end = singles.end();
    for( ; it != it_end; ++it )
    {
        SingleMarker single;
        single.id = (int)(*it)["id"];
        single.size = (float)(*it)["size"];
        single.isStatic = (bool)(int)(*it)["isStatic"];
        this->singleMarkers[single.id] = single;
    }

    cv::FileNode bodies = node["markerBodies"];
    it = bodies.begin(), it_end = bodies.end();
    for( ; it != it_end; ++it )
    {
        MarkerBody body;
        (*it) >> body;
        this->markerBodies[body.name] = body;
        std::cout << "Body:" << "\n";
        std::cout << " name:     " << body.name << "\n";
        std::cout << " isStatic: " << body.isStatic << "\n";
        for (std::map<int, MarkerTransform>::iterator m = body.markers.begin(); m != body.markers.end(); ++m) {
            std::cout << " marker: " << m->first << " - " << m->second.matrix << "\n";
        }
    }

    cv::FileNode cameras = node["cameras"];
    it = cameras.begin();
    it_end = cameras.end();
    for( ; it != it_end; ++it )
    {
        std::string name = (*it)["name"];
        std::string url = (*it)["calibrationUrl"];
        std::stringstream s;

        if (url == "none") {
            //do not use a calibration file
            //this makes only sense when the calibration is set during runtime (e.g. in the ros node)
            s << url;
        }else if (url.at(0) == '/'){
            //this is an absolute path, do not prepend working dir!
            s << url;
		}
		else if (workingDir == ""){
			s << url;
		} 
		else
		{
            //prepend working dir
            s << workingDir << "/" << url;
        }

        bool isStatic = (bool)(int)(*it)["isStatic"];
        this->cameras[name] = Camera(name, s.str(), isStatic);
        std::cout << "Camera:" << "\n";
        std::cout << " name:     " << name << "\n";
        std::cout << " url:     " << url << "\n";
        std::cout << " isStatic: " << this->cameras[name].isStatic << "\n";
    }
}

