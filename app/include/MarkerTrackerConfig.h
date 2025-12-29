/* 
 * File:   MarkerTrackerConfig.h
 * Author: prenner
 *
 * Created on May 12, 2014, 7:17 PM
 */

#ifndef MARKERTRACKERCONFIG_H
#define	MARKERTRACKERCONFIG_H

#include<iostream>

#include "Camera.h"
#include "MarkerBody.h"

class MarkerTrackerConfig
{
public:
    
    MarkerTrackerConfig();

    void write(cv::FileStorage& fs) const;
    void read(const cv::FileNode& node, std::string workingDir); 
    void read(const cv::FileNode& node); 
    
    std::string toString() {
        std::stringstream str;
        str<< "MarkerTrackerConfig(" 
            << "debugging: " << debugging
            << "debug_images:" << debug_images
            << "; decayTime: " << decayTime
            << "; timeout: " << timeout
            << "; scales: " << scales
            << "; downScaleFactor: " << downScaleFactor
			<< "; undistortImage: " << undistortImage
            << "; useMedianFlow: " << useMedianFlow
            << "; minContourLengthAllowed: " << minContourLengthAllowed
            << "; roiScalePrior: " << roiScalePrior
            << "; roiScaleRefinement: " << roiScaleRefinement
            << "; minMarkerSizeInPixels: " << minMarkerSizeInPixels
            << "; maxNumberOfMarkers: " << maxNumberOfMarkers
            << "; maxThreads: " << maxThreads
            << "; markerSizePixels: " << markerSizePixels.width << ", " << markerSizePixels.height
            << "; markerSize: " << markerSize.width << ", " << markerSize.height
			<< "; dejitterEpsilon: " << dejitterEpsilon 
			<< "; dejitterDamping: " << dejitterDamping
            << ")";
        return str.str();
}
    
    // Data Members
    bool debugging;
    bool debug_images;
    unsigned int timeout;
    unsigned int scales;
    float downScaleFactor;
    size_t decayTime;
	bool undistortImage;
	bool useMedianFlow;
	float minContourLengthAllowed;
    float roiScalePrior;
    float roiScaleRefinement;
	int   minMarkerSizeInPixels;
	int   maxNumberOfMarkers;
	int   maxThreads;
	int   throttle;
	cv::Size markerSizePixels;
	cv::Size2f markerSize;
	float dejitterEpsilon;
	float dejitterDamping;
    
    std::map<std::string, Camera> cameras; 
    std::map<int,SingleMarker> singleMarkers;
    std::map<std::string, MarkerBody> markerBodies;
    bool useOTSUThreshold;
};



// --- Outer-class Methods for reading the config ---

static void 
write(cv::FileStorage& fs, const std::string&, const MarkerTrackerConfig& config)
{
    config.write(fs);
}

static void 
read(const cv::FileNode& node, MarkerTrackerConfig& config, const MarkerTrackerConfig& default_value = MarkerTrackerConfig())
{
if(node.empty())
    config = default_value;
else
    config.read(node);
}

#endif	/* MARKERTRACKERCONFIG_H */

