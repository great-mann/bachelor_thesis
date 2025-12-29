/*
 * MultimarkerTracker for tracking rigid bodies of markers.
 * author: prenner
 */

#include "MultiMarkerTracker.h"
#include "GeometryHelpers.h"
#include "MarkerBody.h"
//#include "opencv2/imgproc/types_c.h"
//#include "opencv2/calib3d.hpp"

#include "Logger.h"



#define MM_TO_M 0.001f;

MultiMarkerTracker::MultiMarkerTracker(size_t decayTime) 
{
    this->decayTime = decayTime; // default value
    setDecayTime(decayTime);
}


// ------------------------------------------------------------------------------------------
// ----------------------- Camera and Marker body management --------------------------------
// ------------------------------------------------------------------------------------------

void
MultiMarkerTracker::addCamera(Camera &camera) {
    LOG(debug) << "Adding predefined camera " << camera.name << "." << "\n";
    this->cameras[camera.name] = MultiCamera(camera.name, camera.camMatrix, camera.distCoeff, camera.isStatic, camera.resolution);
    this->cameras[camera.name].detectedMarkers.setDecayTime(this->decayTime);
    this->cameras[camera.name].lastframeTimestamp = boost::posix_time::ptime(boost::posix_time::neg_infin);
}

void
MultiMarkerTracker::addMarkerBody(MarkerBody &body) {
    this->bodies[body.name] = body;
    for (std::map<int, MarkerTransform>::iterator m = body.markers.begin(); m != body.markers.end(); ++m) {
        this->idBodyMap[m->first] = body.name;
    }
}

void 
MultiMarkerTracker::addSingleMarker(SingleMarker& single) {
    this->singleMarkers[single.id] = single;
}



// ------------------------------------------------------------------------------------------
// ----------------------- Methods for getting likely markers -------------------------------
// ------------------------------------------------------------------------------------------

void
MultiMarkerTracker::newFrame(std::string camera, boost::posix_time::ptime frameTime) {
    if (this->cameras.find(camera) == this->cameras.end()) {
        LOG(warning) << "Adding camera " << camera << " without calibration!" << "\n";
        this->cameras[camera] = MultiCamera(camera, cv::Mat(), cv::Mat(), false, cv::Size(0,0));
        this->cameras[camera].lastframeTimestamp = boost::posix_time::ptime(boost::posix_time::neg_infin);
    }

    if (this->cameras[camera].lastframeTimestamp < frameTime){
        LOG(debug) << "New frame of camera " << camera << "!" << "\n";

		// Save dynamic marker bodies
		//this->cameras[camera].lastDynamicBodies = getDynamicBodyTransformations(camera);
		//std::map<std::string, cv::Mat> lastBodies = getDynamicBodyTransformations(camera);
//		for (std::map<std::string, cv::Mat>::iterator lIt = lastBodies.begin(); lIt != lastBodies.end(); ++lIt) {
//			this->cameras[camera].lastDynamicBodies[lIt->first] = lIt->second;
//		}

        //this->likelyMutex[camera]->lock();
        //this->cameras[camera].likelyMarkers.clear();
        //this->likelyMutex[camera]->unlock();
        this->detectedMutex.lock();
        std::map<int,Marker> lastDetections =
                this->cameras[camera].detectedMarkers.get(this->cameras[camera].lastframeTimestamp);
        this->detectedMutex.unlock();
        
        std::map<std::string, std::vector<Marker> > insertionMap;
        for (std::map<int, Marker>::iterator m = lastDetections.begin(); m != lastDetections.end(); ++m) {
            (m->second).source = "previous";
            insertionMap[getBodyNameByMarkerId(m->first)].push_back(m->second); 
            LOG(debug) << " -- Adding marker " << m->first << "(body " << getBodyNameByMarkerId(m->first) << ")" << "\n";
        }

        //add markers from kalman filtering step
        boost::posix_time::ptime timestamp = cameras[camera].lastframeTimestamp;
        std::map<int,Marker> kalmanEstimations = kalmanMarkerListMap[camera].estimateMarkers(frameTime);

        LOG(debug) << "adding " << kalmanEstimations.size() << " kalman estimated markers for camera " << camera << " to likelyMarkers\n";
        for (std::map<int, Marker>::iterator m = kalmanEstimations.begin(); m != kalmanEstimations.end(); ++m) {
            (m->second).source = "kalman";
            insertionMap[getBodyNameByMarkerId(m->first)].push_back(m->second);
            LOG(debug) << " -- Adding kalman estimated marker " << m->first << "(body " << getBodyNameByMarkerId(m->first) << ")" << "\n";
        }
        
        
        // Make the following function calls atomic using recursive lock
        {
            this->cameras[camera].hypothesisManager[frameTime].startAtomicHypothesisOperation();
            
            // Remove old previous and kalman markers
            this->cameras[camera].hypothesisManager[frameTime].removeHypotheses("previous");
            this->cameras[camera].hypothesisManager[frameTime].removeHypotheses("kalman");
            // Add new likely markers to the cameras map
            this->cameras[camera].hypothesisManager[frameTime].addHypotheses(insertionMap);
            // Sort new likely markers
            this->cameras[camera].hypothesisManager[frameTime].sortHypotheses();
            
            this->cameras[camera].hypothesisManager[frameTime].endAtomicHypothesisOperation();
        }
        
    }
}



bool
MultiMarkerTracker::getNextLikelyMarker(std::string camera, boost::posix_time::ptime &frameTime, Marker &marker) {
    LOG(debug) << "Get next likely marker for camera " << camera << "...\n";
    
    if (this->cameras[camera].hypothesisManager[frameTime].getNextHypothesis(marker)) {
        LOG(debug) << "Found marker " << marker.id << " in body " << getBodyNameByMarkerId(marker.id) << ".\n";
        return true;
    }
    return false;
}

void MultiMarkerTracker::estimationFrameFinished(std::string camera, boost::posix_time::ptime timestamp){
    kalmanMarkerListMap[camera].frameFinished(timestamp);
}

KalmanMarkerList::kalmanmarkermap_t MultiMarkerTracker::estimateMarkerPositions(std::string camera, boost::posix_time::ptime timestamp){
    return kalmanMarkerListMap[camera].estimatedMarkerPositions(timestamp);
}


// ------------------------------------------------------------------------------------------
// ----------------------- Methods for handling detected markers ----------------------------
// ------------------------------------------------------------------------------------------

void
MultiMarkerTracker::detectedMarker(std::string camera, 
                                     Marker marker, 
                                     boost::posix_time::ptime frameTimestamp, 
                                     cv::Size frameSize) 
{
    LOG(debug) << "Detected marker " << marker.id;
    
	// Check if we had that one already
	std::map<int, Marker> curDetectedMarkers = this->detectedMarkers.get(0);
	if (curDetectedMarkers.find(marker.id) != curDetectedMarkers.end()) {
        LOG(debug) << "...it was already detected!";
		return;
	}

    kalmanMarkerListMap[camera].update_marker(marker, frameTimestamp);

    // First check if the marker is unknown and the global transform can be estimated
    if (getBodyNameByMarkerId(marker.id) == "") {
        // If a transform could be estimated, use a weighted average
        if (!estimateGlobalTransform(camera, marker, frameTimestamp).empty()) {
            markerQualities[camera][marker.id] = calcTransformationQuality(marker, camera);
        }
    } 
    // If the marker is known, check for possible updates of the unknown ones later
    // but calculate transformation quality 
    else if (hasStaticTransformation(marker.id) || !getBodyByMarkerId(marker.id).isStatic) {
        markerQualities[camera][marker.id] = calcTransformationQuality(marker, camera);
        markerGenerations[marker.id] = 0; // Static marker -> generation 0
    }

	// Remove from likely markers
	std::string bodyName = getBodyNameByMarkerId(marker.id);
	this->cameras[camera].hypothesisManager[frameTimestamp].removeHypotheses(marker.id, bodyName);
    
    // Behaviour for a new frame
    if (this->cameras[camera].lastframeTimestamp < frameTimestamp) {
        this->detectedMutex.lock();
        this->cameras[camera].detectedMarkers.insert(frameTimestamp, marker.id, marker);
        this->detectedMarkers.insert(frameTimestamp, marker.id, marker);
        this->cameras[camera].lastframeTimestamp = frameTimestamp;
        this->detectedMutex.unlock();
        if (!this->cameras[camera].camMatrix.empty()) {
            calcLikelyMarkers(camera, frameTimestamp, marker, frameSize);
		} else {
            LOG(debug) << "No camera matrix (camera '" << camera << "')! " << "\n";
		}

    // Behavior for the current or an older frame
    } else {
        this->detectedMutex.lock();
        this->cameras[camera].detectedMarkers.insert(frameTimestamp, marker.id, marker);
        
        this->detectedMarkers.insert(frameTimestamp, marker.id, marker);
        
        // Check if new marker is bigger than the previous in the body TODO: Use quality
        bool better = false;
        bool previousKnown = false;
        if (hasStaticTransformation(marker.id)) {
            std::map<int,Marker> camDetections = this->cameras[camera].detectedMarkers.get(frameTimestamp);
            std::map<int, Marker>::iterator m = camDetections.begin();
            for ( ; m != camDetections.end(); ++m) {
                if (getBodyNameByMarkerId(m->first) != "") {
                    previousKnown = true;
                }
                if (m->second < marker) {
                    // If yes, recalculate likely markers
                    if (!this->cameras[camera].camMatrix.empty()) {
                        better = true;
                    }
                    break;
                }
            }
        }
        this->detectedMutex.unlock();
        
        // If the new marker is bigger, recalculate likely markers
        if (better || !previousKnown) {
            calcLikelyMarkers(camera, frameTimestamp, marker, frameSize);
        } 
        
        // Remove detected id from likely markers
        std::string bodyName = getBodyNameByMarkerId(marker.id);
        this->cameras[camera].hypothesisManager[frameTimestamp].removeHypotheses(marker.id, bodyName);
        
        
        // Look for previously detected unknown markers
        if (getBodyNameByMarkerId(marker.id) != "" && hasStaticTransformation(marker.id)) {
            LOG(debug) << "  Marker " << marker.id << " was found in body " << getBodyNameByMarkerId(marker.id) << "...\n";
            std::map<int,Marker> previousMarkers = getDetectedMarkers(frameTimestamp);
            LOG(debug) << "  Found " << previousMarkers.size() << " former detections." << "...\n";
            for (std::map<int,Marker>::iterator mIt = previousMarkers.begin(); mIt != previousMarkers.end(); ++mIt) {
                LOG(debug) << "   - Marker " << mIt->first;
                if (getBodyNameByMarkerId(mIt->first) == "")  {
                    LOG(debug) << "       (unknown) " << "...\n";
                    if (!estimateGlobalTransform(camera, mIt->second, frameTimestamp).empty()) {
                        markerQualities[camera][mIt->first] = calcTransformationQuality(mIt->second, camera);
                    }
                } else {
                    LOG(debug) << "       (known) " << "...\n";
                }
            }
        }
    }
}

std::map<int, Marker> 
MultiMarkerTracker::getDetectedMarkers(boost::posix_time::ptime timestamp) {
    std::map<int, Marker> retMap;
    this->detectedMutex.lock();
    if (!this->detectedMarkers.empty(timestamp)) {
        std::map<int, Marker> tmpMap = this->detectedMarkers.get(timestamp);
        if (!tmpMap.empty()) {
            retMap.insert(tmpMap.begin(), tmpMap.end());
        }
    }
    this->detectedMutex.unlock();
    
    return retMap;
}

boost::posix_time::ptime 
MultiMarkerTracker::getDetectedMarkers(size_t milliseconds, std::map<int, Marker>& markerMap) {
    boost::posix_time::ptime timestamp;
    this->detectedMutex.lock();
    if (!this->detectedMarkers.empty()) {
        std::map<int, Marker> tmpMap;
        timestamp = this->detectedMarkers.get(milliseconds, tmpMap);
        if (!tmpMap.empty()) {
            markerMap.insert(tmpMap.begin(), tmpMap.end());
        }
    }
    this->detectedMutex.unlock();
    
    return timestamp;
}

std::map<int, Marker> 
MultiMarkerTracker::getDetectedMarkers(size_t milliseconds) {
    std::map<int, Marker> retMap;
    this->detectedMutex.lock();
    if (!this->detectedMarkers.empty()) {
        std::map<int, Marker> tmpMap = this->detectedMarkers.get(milliseconds);
        if (!tmpMap.empty()) {
            retMap.insert(tmpMap.begin(), tmpMap.end());
        }
    }
    this->detectedMutex.unlock();
    
    return retMap;
}

std::map<std::string, cv::Mat>
MultiMarkerTracker::getDynamicBodyTransformations(std::string& camera, boost::posix_time::ptime& timestamp) {
    
    std::map<std::string, cv::Mat> bodyTransformations;
        
    std::map<int, Marker> detected = this->cameras[camera].detectedMarkers.get(timestamp);
    if (detected.empty()) {
        return bodyTransformations;
    }
    
    LOG(debug) << "BBB getDynCall " << timestamp << "\n";
    for (std::map<std::string,MarkerBody>::iterator bIt = this->bodies.begin(); bIt != this->bodies.end(); ++bIt) {
        
        // Don't use the undefined dummy body!
        if (bIt->first == "") {
            continue;
        }
        
        LOG(debug) << "Getting body " << bIt->first;
        if (!bIt->second.isStatic) {
            
//			std::map<float, Marker> sortedMarkers; 
//            
//                bool found = false;
//				float norm;
//				for (std::map<int,Marker>::iterator mIt = detected.begin(); mIt != detected.end(); ++mIt) {
//                    LOG(debug) << "Getting marker " << mIt->first;
//                    if (getBodyNameByMarkerId(mIt->first) == bIt->first) {
//						cv::Mat cameraMat = mIt->second.cameraTransformation;
//                        if (!cameraMat.empty()) { // Only known markers with camera transformation are relevant
//							found = true;
//							float curQuality = this->markerQualities[camera][mIt->first];
//							norm = cv::norm(cv::Vec3f(cameraMat.at<float>(3,0),
//															cameraMat.at<float>(3,1),
//															cameraMat.at<float>(3,2)));
//							int insertions = (curQuality * 2) + 1;
//							while (insertions-- > 0) {
//                                LOG(debug) << "insertions: " << insertions;
//								sortedMarkers[norm] = mIt->second;
//							}
//							
//                        }
//                    }
//                }
//
//                if (found) {
//					std::map<float, Marker>::iterator it = sortedMarkers.begin();
//					std::advance(it, sortedMarkers.size()/2);
//					Marker best = it->second;
//					bodyTransformations[bIt->first] = best.cameraTransformation; 
//                }
                
                std::map<int,Marker> bodyMarkers;
                for (std::map<int,Marker>::iterator mIt = detected.begin(); mIt != detected.end(); ++mIt) {
                    if (getBodyNameByMarkerId(mIt->first) == bIt->first) {
                        bodyMarkers[mIt->first] = mIt->second;
                    }
                }
            
                cv::Mat transformation;
                calcCombinedTransformation(camera, transformation, bodyMarkers, false, cv::Mat(), cv::Mat());
                if (!transformation.empty()) {
                    bodyTransformations[bIt->first] = transformation;
                }
                
            }
    }
    
    // Save the bodies
    for (std::map<std::string, cv::Mat>::iterator lIt = bodyTransformations.begin(); lIt != bodyTransformations.end(); ++lIt) {
        this->cameras[camera].lastDynamicBodies[lIt->first] = lIt->second;
    }
    
    LOG(debug) << "BBB getDynCall done.";
    
    return bodyTransformations;
}

std::map<std::string, cv::Mat> 
MultiMarkerTracker::getDynamicBodyTransformations(std::string &camera, size_t milliseconds) {
    std::map<int, Marker> detected;
    boost::posix_time::ptime detectedTimestamp = this->cameras[camera].detectedMarkers.get(milliseconds, detected);
    return getDynamicBodyTransformations(camera, detectedTimestamp);
}

float 
MultiMarkerTracker::getCameraTransformation(std::string &camera, cv::Mat &transformation, cv::Mat &image, boost::posix_time::ptime &timestamp) {

    std::map<int, Marker> detected = this->cameras[camera].detectedMarkers.get(timestamp);

    // Calculate transformation
    float res = calcCombinedTransformation(camera, transformation, detected, true, this->cameras[camera].lastRVec, this->cameras[camera].lastTVec);
    
    // Recalculate likely markers from the best transformation
    cv::Size imageSize(image.cols, image.rows);
    if (!transformation.empty()) {
        calcLikelyMarkers(camera, timestamp, transformation, imageSize);
    }

    return res;
    
}


float MultiMarkerTracker::calcCombinedTransformation(
        std::string& camera, 
        cv::Mat& transformation, 
        std::map<int, Marker>& detectedMarkers, 
        bool global, 
        cv::Mat rVecPrior, 
        cv::Mat tVecPrior) 
{

    float quality = -1.0f;
    if ((detectedMarkers.size() == 0) || (detectedMarkers.empty())) {
        return quality;
    } 

    // Find best detected marker
    Marker best = detectedMarkers.begin()->second;
    bool foundStatic = false;
    for (std::map<int,Marker>::iterator mIt = detectedMarkers.begin(); mIt != detectedMarkers.end(); ++mIt) {
        if (!mIt->second.cameraTransformation.empty()) { // Only known markers with camera transformation are relevant
            float curQuality = this->markerQualities[camera][mIt->first];
            bool tmpIsStatic = (global && getBodyNameByMarkerId(mIt->first) != ""  && getBodyByMarkerId(mIt->first).isStatic) || (!global && getBodyNameByMarkerId(mIt->first) != "");
            if ( (tmpIsStatic && !foundStatic) // static is always preferred
                      || (((tmpIsStatic && foundStatic) || !foundStatic) && curQuality > quality) ) 
            {  
                foundStatic = tmpIsStatic;
                best = mIt->second;
                quality = curQuality;
            }
        }
    }
    if ((global && getBodyByMarkerId(best.id).isStatic) || (!global && !best.cameraTransformation.empty())) {

        transformation = best.cameraTransformation; 
    }

	LOG(debug) << "Returning best camera transformation from marker " << best.id;
    LOG(debug) << transformation;
    LOG(debug) << "Quali " << quality  << "\n";

    // Check if adequate markers were found 
    if (getBodyNameByMarkerId(best.id) == "") {
            return -1;
    } else if (global
            && getBodyNameByMarkerId(best.id) != ""
            && !getBodyByMarkerId(best.id).isStatic) {
            return -1;
    }

    // Directly return if there is only one marker
    if (detectedMarkers.size() <= 1) {
        return quality;
    }


    
    // Use all detections as a huge marker
    // Use extrinsic guess from best marker
    cv::Mat tExt = cv::Mat(cv::Point3f(transformation.at<float>(3,0), transformation.at<float>(3,1), transformation.at<float>(3,2)));
    cv::Mat rMat = transformation(cv::Rect(0,0,3,3));
    cv::Mat_<float> rExtPre(3,1);
    cv::Mat_<float> rExt(3,1);
    cv::Rodrigues(rMat, rExtPre);
    tExt.at<float>(1) *= -1.0f;  // invert y-axis for cv coordinate frame
    tExt.at<float>(2) *= -1.0f;  // invert z-axis for cv coordinate frame
    tExt *= 1000.0f;               // transfrom meter coordinates to mm
    rExt.at<float>(0) = rMat.at<float>(0);
    rExt.at<float>(1) = rMat.at<float>(1);
    rExt.at<float>(2) = rMat.at<float>(2);
    rExt.at<float>(0) *= -1.0f; // invert x-axis for CV coordinate frame
    
    
    LOG(debug) << "Calculating global camera transformation (new approach)...";
    
    cv::Mat Rvec;
    cv::Mat_<float> Tvec;
    cv::Mat raux,taux;
    cv::Mat mm3;
    cv::Mat m2;
    std::vector<cv::Point2f> imagePoints;
    std::vector<cv::Point3f> rectifiedPoints;
    
    
    for (std::map<int,Marker>::iterator mIt = detectedMarkers.begin(); mIt != detectedMarkers.end(); ++mIt) {
        Marker marker = mIt->second;
        LOG(debug) << "Marker " << marker.id << " quality:" << this->markerQualities[camera][marker.id];
        if ((global && !getBodyByMarkerId(marker.id).isStatic) || (!global && marker.cameraTransformation.empty()) ) {
            LOG(debug) << " -> Marker is not usable.";
            continue;
        }
        LOG(debug) << "Marker " << marker.id << " size:" << marker.size.width << "," << marker.size.height;
    
        // Get marker transformation
        MarkerTransform mt;
        getMarkerTransformation(marker.id, mt);
        cv::Vec3f pos = mt.getPosition();
        pos[1] *= -1.0f;
        pos[2] *= -1.0f;
        pos *= 1000; // M_TO_MM
        cv::Vec4f ori = mt.getOrientation();
        cv::Mat transformationMat = GeometryHelpers::constructMat(pos, ori);
        LOG(debug) << " transformationMat: " << "\n" << transformationMat;
        
        // Insert image points
        std::vector<cv::Point2f> imP = marker.calculatedGoodFeaturesToTrack;
        imagePoints.insert(imagePoints.end(), imP.begin(), imP.end());
        
        // Insert rectified points
        std::vector<cv::Point3f> reP = marker.rectifiedGoodFeaturesToTrack;
        for (unsigned int i = 0; i < reP.size(); ++i) {
            reP[i] = GeometryHelpers::multPointMatrix(reP[i], transformationMat);
        }
        rectifiedPoints.insert(rectifiedPoints.end(), reP.begin(), reP.end());
    
    }
    
    // Check if there was any usable marker
    if (rectifiedPoints.size() == 0) {
        return -1;
    }
    
 
    
    // Calculate transformation
    mm3 = cv::Mat(rectifiedPoints);
    LOG(debug) << "Calc 3D Points:" << "\n" << mm3 << "\n";
    m2 = cv::Mat(imagePoints);
    LOG(debug) << "Calc 2D Points:" << "\n" << m2 << "\n";
    
    
    cv::Mat inliers;
    if (cv::countNonZero(rVecPrior) >= 1 && cv::countNonZero(tVecPrior) >= 1) {
        raux = rVecPrior;
        taux = tVecPrior;
        LOG(debug) << "Using extrinsic guess.";
        //cv::solvePnPRansac(mm3, m2, this->cameras[camera].camMatrix, this->cameras[camera].distCoeff,raux,taux,true,
        //                100, 2.0, imagePoints.size()*0.5, inliers, cv::ITERATIVE);
    } else {
        LOG(debug) << "Not using extrinsic guess.";
        //cv::solvePnPRansac(mm3, m2, this->cameras[camera].camMatrix, this->cameras[camera].distCoeff,raux,taux,false,
        //                100, 2.0, imagePoints.size()*0.5, inliers, cv::ITERATIVE);
    }
    cv::solvePnP(mm3, m2, this->cameras[camera].camMatrix, this->cameras[camera].distCoeff,raux,taux);

//    // Display points 
//    cv::Mat pointImg = cv::Mat::zeros(this->cameras[camera].resolution.width,this->cameras[camera].resolution.height, CV_8UC3);
//    for (unsigned int i = 0; i < inliers.rows; ++i) {
//        cv::circle(pointImg, imagePoints[inliers.at<int>(i)], 3, cv::Scalar(0,255,0), -1);
//    }
//    cv::imshow("Points", pointImg);
//    cv::waitKey(1);
    
    rVecPrior = raux;
    tVecPrior = taux;

    raux.convertTo(Rvec,CV_32F);
    taux.convertTo(Tvec ,CV_32F);
    Tvec *= MM_TO_M;             // transfrom millimeter coordinates to meter
    Tvec.at<float>(1) *= -1.0f;  // invert x-axis for InstantReality coordinate frame
    Tvec.at<float>(2) *= -1.0f;  // invert x-axis for InstantReality coordinate frame
    
    Rvec.at<float>(0) *= -1.0f;  // invert x-axis for InstantReality coordinate frame

    cv::Mat_<float> rotMat(3,3);

    cv::Rodrigues(Rvec, rotMat);
    
    cv::Mat trans = cv::Mat::eye(4, 4, CV_32F);
    rotMat.copyTo(trans(cv::Rect(0,0,3,3)));
    cv::transpose(Tvec, trans(cv::Rect(0,3,3,1)));

    // Copy to transformation matrix
//    Transformation globalTransformation;
//    for (int col=0; col<3; col++)
//    {
//        for (int row=0; row<3; row++)
//        {        
//            globalTransformation.r().mat[row][col] = rotMat(row,col); // Copy rotation component
//        }
//        globalTransformation.t().data[col] = Tvec(col); // Copy translation component
//    }
    // Since solvePnP finds camera location, w.r.t to marker pose, to get marker pose w.r.t to the camera we invert it.
    transformation = trans.inv();
    
	LOG(debug) << " Global transformation is " << transformation << "\n";
    LOG(debug) << " Inverted global transformation is " << transformation.inv() << "\n";
    
    return 1;
}


float 
MultiMarkerTracker::getCameraTransformation(std::string &camera, cv::Mat &transformation, cv::Mat &image, size_t milliseconds) {
    std::map<int, Marker> detected;
    boost::posix_time::ptime detectedTimestamp = this->cameras[camera].detectedMarkers.get(milliseconds, detected);
    return getCameraTransformation(camera, transformation, image, detectedTimestamp);
}


std::pair<float,unsigned int> 
MultiMarkerTracker::getConstrainedCameraTransformation(std::string& camera, 
                                                           cv::Mat& transformation, 
                                                           boost::posix_time::ptime& timestamp, 
                                                           unsigned int maxGeneration) 
{
    std::map<int, Marker> detected = this->cameras[camera].detectedMarkers.get(timestamp);
    float quality = -1.0f;
    int generation = -1.0f;
    if (detected.size() == 0) {
        LOG(debug) << "Could not find a fitting cam transformation! ";
        return std::pair<float, unsigned int>(quality,generation);
    } 
    Marker best = detected.begin()->second;
    if (!detected.empty()) {
        // Find biggest detected marker
        bool foundStatic = false;
        for (std::map<int,Marker>::iterator mIt = detected.begin(); mIt != detected.end(); ++mIt) {
            if (mIt->second.hasCameraTransformation) { // Only known markers with camera transformation are relevant
                if (this->markerGenerations.find(mIt->first) != this->markerGenerations.end() 
                        && this->markerGenerations[mIt->first] <= maxGeneration) 
                {
                    float curQuality = this->markerQualities[camera][mIt->first];
                    bool tmpIsStatic = getBodyNameByMarkerId(mIt->first) != "";
                    if ( (tmpIsStatic && !foundStatic) // static is always preferred
                              || (((tmpIsStatic && foundStatic) || !foundStatic) && curQuality > quality) ) 
                    {  
                        foundStatic = tmpIsStatic;
                        best = mIt->second;
                        quality = curQuality;
                    }
                }
            }
        }
        if (best.hasCameraTransformation) {
            transformation = best.cameraTransformation;
            generation = this->markerGenerations[best.id];
        }
    }
    
    LOG(debug) << "Returning best camera transformation from marker " << best.id;
    LOG(debug) << "  -> Generation is " << generation << "...\n";
    LOG(debug) << "  -> Transformation is " << transformation << "...\n";
    return std::pair<float, unsigned int>(quality,generation);
}


bool
MultiMarkerTracker::getMarkerTransformation(int id, MarkerTransform& transform) {
    std::string name = getBodyNameByMarkerId(id);
    if (name != "") {
        transform = this->bodies[name].markers[id];
        return true;
    } else if (this->estimatedMarkerTransformations.find(id) != this->estimatedMarkerTransformations.end()) {
        transform.id = id;
        transform.matrix = this->estimatedMarkerTransformations[id];
        return true;
    }
    return false;
}

std::map<int,cv::Mat> MultiMarkerTracker::getSingleMarkerTransformations() {
    return this->estimatedMarkerTransformations;
}



// ------------------------------------------------------------------------------------------
// ----------------------- Private methods --------------------------------------------------
// ------------------------------------------------------------------------------------------


// ------------------------------------------------------------------------------------------
// -------------- Methods for calculating relations between markers and bodies --------------
// ------------------------------------------------------------------------------------------

cv::Mat 
MultiMarkerTracker::estimateGlobalTransform(std::string &camera, Marker &marker, boost::posix_time::ptime &timestamp) {
    cv::Mat globalTransform;
    cv::Mat cameraTransform;
    int generation = 999;
    if (this->markerGenerations.find(marker.id) != this->markerGenerations.end()) {
        generation = this->markerGenerations[marker.id];
    }
    std::pair<float,unsigned int> retVal = getConstrainedCameraTransformation(camera, cameraTransform, timestamp, generation);
    float quality = retVal.first;
    generation = retVal.second;
    if (quality != -1 && !cameraTransform.empty()) {
        this->markerGenerations[marker.id] = generation + 1;
        LOG(debug) << "Estimating transform for unknown marker " << marker.id << "...\n";
        cv::Mat markerMat = marker.transformation;
        globalTransform = markerMat * cameraTransform;
        if (estimatedMarkerTransformations.find(marker.id) != estimatedMarkerTransformations.end()) {
            //LOG(debug) << "  Interpolating with factor " << 1.0f/estimatedMarkerIterations[marker.id] << "...\n";
            LOG(debug) << "  Interpolating with factor " << quality << "...\n";
            this->estimatedMarkerTransformations[marker.id] = 
                    GeometryHelpers::interpolate(estimatedMarkerTransformations[marker.id], 
                                                 globalTransform, 
                                                 quality); 
            ++this->estimatedMarkerIterations[marker.id];
        } else {
            this->estimatedMarkerTransformations[marker.id] = globalTransform;
            this->estimatedMarkerIterations[marker.id] = 1;
        }
        LOG(debug) << "  Transform: " << this->estimatedMarkerTransformations[marker.id] << "...\n";
        return this->estimatedMarkerTransformations[marker.id];
    }
    return globalTransform;
}

float 
MultiMarkerTracker::calcTransformationQuality(Marker& marker, std::string &camera) {
    
    // TODO: We are making some assumptions here...
    
    LOG(debug) << "Quality of marker " << marker.id << ": ";
    
//    // Calc relative area in the image
//    cv::Size resolution = this->cameras[camera].resolution;
//    LOG(debug) << "   resolution = " << resolution.width << "," << resolution.height;
//    LOG(debug) << "   imageSize = " << (float)marker.getImageSize();
//    float relativeSize = std::min(1.0f, ((float)marker.getImageSize()) / ( 50000.0f )); // Say a good size is 10000 pixels
//    LOG(debug) << "   -> relativeSize = " << relativeSize;
//    
//    // Calc cos of the angle between camera and marker
//    cv::Mat rotMat = marker.transformation(cv::Rect(0,0,3,3));
//    cv::Vec3f euler = GeometryHelpers::rotationMat2EulerAngles(rotMat);
//    LOG(debug) << "   euler = " << euler[0] << "," << euler[1] << "," << euler[2];
//    LOG(debug) << "   euler cosAngle[0] = " << cos(euler[0]);
//    LOG(debug) << "   euler cosAngle[1] = " << cos(euler[1]);
//    LOG(debug) << "   euler cosAngle[2] = " << cos(euler[2]) << " (ignored)";
//    
//    // Take the minumum angle as quality measure
//    float minAngle = std::min(cos(euler[0]) , cos(euler[1]));
//    LOG(debug) << "   -> min cosAngle = " << minAngle;
//    
//    // TODO: It seems(!) like size is way more important, thus we weight it with 3:1
//    float quality = (relativeSize*5.0f + minAngle) / 6.0f;
//    LOG(debug) << " ---> quality (weight 5:1) = " << quality;
    cv::Mat t = marker.transformation(cv::Rect(0,3,3,1));
    float norm = cv::norm(t);
    
    float quality = 1.0f - norm/4.0f;
    marker.quality = quality;
    
    return quality;
}




// ------------------------------------------------------------------------------------------
// ----------------------- Methods for calculating likely markers ---------------------------
// ------------------------------------------------------------------------------------------

std::vector<cv::Vec3f>
MultiMarkerTracker::calcRelativeMarkerPositions(int id, MarkerBody &markerBody, cv::Vec3f position) {
    // Vector of 3D marker positions relative to the input marker
    std::vector<cv::Vec3f> relativePositions;

    // Get the global position of the given marker
    cv::Vec3f globalPosIn = markerBody.markers[id].getPosition();

    // Calculate them
    for (std::map<int, MarkerTransform>::iterator marker = markerBody.markers.begin();
            marker != markerBody.markers.end();
            ++marker) {
        if (marker->first == id) {
            continue; // skip the input marker
        }
        // Calculate the global 3D position of the other markers
        relativePositions.push_back(position + (marker->second.getPosition() - globalPosIn));
    }

    return relativePositions;
}

std::vector<cv::Point2f>
MultiMarkerTracker::calcRelativeMarkerCorners(std::string camera, 
                                                cv::Mat &originMat, 
                                                cv::Mat &targetMat, 
                                                cv::Size2f &targetSize, 
                                                cv::Mat &transformation) 
{
    return calcRelativeMarkerCorners(camera, targetMat, targetSize, transformation);
}

std::vector<cv::Point2f>
MultiMarkerTracker::calcRelativeMarkerCorners(std::string camera, 
                                                cv::Mat &targetMat, 
                                                cv::Size2f &targetSize, 
                                                cv::Mat &transformation) 
{
    
    std::vector<cv::Point2f> imagePoints;

    // Create normed corners 
    std::vector<cv::Vec3f> corners;
    corners.push_back(cv::Vec3f(-targetSize.width / 2.0f, -targetSize.height / 2.0f, 0));
    corners.push_back(cv::Vec3f(targetSize.width / 2.0f, -targetSize.height / 2.0f, 0));
    corners.push_back(cv::Vec3f(targetSize.width / 2.0f, targetSize.height / 2.0f, 0));
    corners.push_back(cv::Vec3f(-targetSize.width / 2.0f, targetSize.height / 2.0f, 0));

    // Calculate transformation of target marker
    cv::Mat targetTransformed = targetMat * transformation.inv();
    
    //Get angle
    cv::Vec3f normalOffset(0, 0, 1.0f);
    cv::Mat normalMat = GeometryHelpers::constructMat(normalOffset, cv::Vec4f());
    normalMat = normalMat * targetTransformed; 
    cv::Vec3f t1, t2, cam;
    GeometryHelpers::decomposeMat(normalMat, t1);
    GeometryHelpers::decomposeMat(targetTransformed, t2);
    GeometryHelpers::decomposeMat(transformation, cam);
    cv::Vec3f normal = t1 - t2;
    // Cancel if the marker is occluded (and a bit more for small angles)
    //LOG(debug) << "    origin: " << t2;
    //LOG(debug) << "    cam: " << cam;
    if (normal[2] < 0.1) { // normal facing backwards?
        LOG(debug) << "    Marker occluded -> normal " << normal[2];
        return imagePoints;
    } else {
        LOG(debug) << "    Marker ok -> normal " << normal[2];
    }
    
    cv::Mat posVec(cv::Vec3f(targetTransformed.at<float>(3, 0), targetTransformed.at<float>(3, 1), targetTransformed.at<float>(3, 2)));
    cv::Mat rotVec;
    cv::Rodrigues(targetTransformed(cv::Rect(0, 0, 3, 3)), rotVec);
    posVec.at<float>(1) *= -1.0f; // invert y-axis for CV coordinate frame
    posVec.at<float>(2) *= -1.0f; // invert z-axis for CV coordinate frame
    posVec *= 1000.0f; // transfrom meter coordinates to millimeter
    cv::Mat rotVecFinal(3, 1, CV_32F);
    rotVecFinal.at<float>(0) = rotVec.at<float>(0);
    rotVecFinal.at<float>(1) = rotVec.at<float>(1);
    rotVecFinal.at<float>(2) = rotVec.at<float>(2);
    rotVecFinal.at<float>(0) *= -1.0f; // invert x-axis for CV coordinate frame
    
    // Project to 2D image coordinates
    cv::projectPoints(corners, rotVecFinal, posVec, this->cameras[camera].camMatrix, this->cameras[camera].distCoeff, imagePoints);

    return imagePoints;
}


std::map<int, std::vector<cv::Point2f> >
MultiMarkerTracker::calcAllRelativeMarkerCorners(std::string camera, int id, MarkerBody &markerBody, cv::Mat &transformation) {
    std::map<int, std::vector<cv::Point2f> > corners;
    LOG(debug) << "Calc rel marker corners.." << "\n";

    // Check if input marker is known
    if (markerBody.markers.find(id) == markerBody.markers.end()) {
        LOG(debug) << "id=" << id << " not in body.." << "\n";
        return corners;
    }

    // Calculate for all markers
    for (std::map<int, MarkerTransform>::iterator marker = markerBody.markers.begin(); marker != markerBody.markers.end(); ++marker) {
        this->detectedMutex.lock();

        std::map<int, Marker> detected = this->detectedMarkers.get(0);
        if (marker->first == id || detected.find(marker->first) != detected.end()) {
            // not for the reference marker and already found markers
            LOG(debug) << "id=" << (marker->first) << " already detected.." << "\n";
            this->detectedMutex.unlock();
            continue;
        }
        this->detectedMutex.unlock();
        // Call the actual method
        MarkerTransform markerTransformOrigin;
        this->getMarkerTransformation(id, markerTransformOrigin);
        std::vector<cv::Point2f> points = calcRelativeMarkerCorners(camera, 
                                                                    markerTransformOrigin.matrix,
                                                                    marker->second.matrix,
                                                                    marker->second.size,
                                                                    transformation);
        if (!points.empty()) {
            corners[marker->first] = points;
        }
    }

    return corners;
}

std::map<int, std::vector<cv::Point2f> >
MultiMarkerTracker::calcAllRelativeMarkerCorners(std::string camera, int id, cv::Mat &transformation) {
    std::map<int, std::vector<cv::Point2f> > corners;

    LOG(debug) << "Calc rel marker corners.." << "\n";

    // Check if input marker is known
    //if (!hasStaticTransformation(id)) { TODO: Check if we can drop this
	if (transformation.empty()) {
        LOG(debug) << id << " has no static transformation.." << "\n";
        return corners;
    }

	std::string originBodyName = getBodyNameByMarkerId(id);
	bool originBodyStatic = getBodyByMarkerId(id).isStatic;
    
    MarkerTransform markerTransformOrigin;
    this->getMarkerTransformation(id, markerTransformOrigin);
    LOG(debug) << "markerTransformOrigin: " << markerTransformOrigin.matrix << "\n";
    
    // Get relevant bodies
    for (std::map<std::string, MarkerBody>::iterator body = this->bodies.begin(); body != this->bodies.end(); ++body) {
		bool otherDynamic = false;
		if ((this->cameras[camera].lastDynamicBodies.find(originBodyName) != this->cameras[camera].lastDynamicBodies.end()) 
				&& (this->cameras[camera].lastDynamicBodies.find(body->first) != this->cameras[camera].lastDynamicBodies.end())) {
			if (originBodyName != body->first) {
				otherDynamic = true;
			}
		}
        if (   (originBodyStatic && body->second.isStatic)
			|| (originBodyName == body->first)
			|| (otherDynamic)) {
            // Calculate for all markers
            for (std::map<int, MarkerTransform>::iterator marker = body->second.markers.begin(); marker != body->second.markers.end(); ++marker) {
                this->detectedMutex.lock();
                std::map<int, Marker> curDetectedMarkers = this->detectedMarkers.get(0);
                this->detectedMutex.unlock();
                if (marker->first == id || curDetectedMarkers.find(marker->first) != curDetectedMarkers.end()) {
                    // not for the reference marker and already found markers
                    LOG(debug) << " -> " << marker->first << " already detected..";
                    continue;
                }

				cv::Mat camTransformation = transformation;
				if (otherDynamic) {
					cv::Mat diffMat = this->cameras[camera].lastDynamicBodies[originBodyName].inv()
								* transformation;
					camTransformation = diffMat * this->cameras[camera].lastDynamicBodies[body->first];
					
				}
                // Call the actual method
                LOG(debug) << " -> " << "Calculating (by body) relative marker corner for marker " << marker->first << "\n";
                std::vector<cv::Point2f> points = calcRelativeMarkerCorners(camera, 
                                                                            markerTransformOrigin.matrix,
                                                                            marker->second.matrix,
                                                                            marker->second.size,
                                                                            camTransformation);
                if (!points.empty()) {
                    corners[marker->first] = points;
                }
            }
        }
    }

    // Calculate for single markers
    for (std::map<int, SingleMarker>::iterator marker = this->singleMarkers.begin(); marker != this->singleMarkers.end(); ++marker) {
        this->detectedMutex.lock();
        std::map<int, Marker> curDetectedMarkers = this->detectedMarkers.get(0);
        this->detectedMutex.unlock();
        if (marker->first == id || curDetectedMarkers.find(marker->first) != curDetectedMarkers.end()) {
            // not for the reference marker and already found markers
            LOG(debug) << " -> " << marker->first << " already detected..";
            continue;
        }
        if (!hasStaticTransformation(marker->first)) {
            // not for markers where we have no transformation yet
            LOG(debug) << " -> " << marker->first << " has no transformation yet..";
            continue;
        }
        MarkerTransform markerTransformTarget;
        this->getMarkerTransformation(marker->first, markerTransformTarget);
        cv::Size2f targetSize(marker->second.size*1000,marker->second.size*1000);
        // Call the actual method
        LOG(debug) << " -> " << "Calculating (by single markers) relative marker corner for marker " << marker->first;
        std::vector<cv::Point2f> points = calcRelativeMarkerCorners(camera,
                                                                    markerTransformOrigin.matrix,
                                                                    markerTransformTarget.matrix, 
                                                                    targetSize,
                                                                    transformation);
        corners[marker->first] = points;
    }

    return corners;
}



std::map<int, std::vector<cv::Point2f> >
MultiMarkerTracker::calcAllMarkerCorners(std::string camera, cv::Mat &transformation) {
    std::map<int, std::vector<cv::Point2f> > corners;

    LOG(debug) << "Calc marker corners.." << "\n";

    // Get relevant bodies
    for (std::map<std::string, MarkerBody>::iterator body = this->bodies.begin(); body != this->bodies.end(); ++body) {
        bool dynamic = (this->cameras[camera].lastDynamicBodies.find(body->first) != this->cameras[camera].lastDynamicBodies.end());
        if (body->second.isStatic) {
            // Calculate for all markers
            for (std::map<int, MarkerTransform>::iterator marker = body->second.markers.begin(); marker != body->second.markers.end(); ++marker) {
                this->detectedMutex.lock();
                std::map<int, Marker> curDetectedMarkers = this->detectedMarkers.get(0);
                this->detectedMutex.unlock();
                if (curDetectedMarkers.find(marker->first) != curDetectedMarkers.end()) {
                    // not for already found markers
                    LOG(debug) << " -> " << marker->first << " already detected..";
                    continue;
                }

                cv::Mat camTransformation = transformation;
//                if (dynamic) {
//                    cv::Mat diffMat = this->cameras[camera].lastDynamicBodies[originBodyName].inv()
//                                            * transformation;
//                    camTransformation = diffMat * this->cameras[camera].lastDynamicBodies[body->first];
//                }
                
                // Call the actual method
                LOG(debug) << " -> " << "Calculating (by global transformation) relative marker corner for marker " << marker->first << "\n";
                std::vector<cv::Point2f> points = calcRelativeMarkerCorners(camera, 
                                                                            marker->second.matrix,
                                                                            marker->second.size,
                                                                            camTransformation);
                if (!points.empty()) {
                    corners[marker->first] = points;
                }
            }
        }
    }

    // Calculate for single markers
    for (std::map<int, SingleMarker>::iterator marker = this->singleMarkers.begin(); marker != this->singleMarkers.end(); ++marker) {
        this->detectedMutex.lock();
        std::map<int, Marker> curDetectedMarkers = this->detectedMarkers.get(0);
        this->detectedMutex.unlock();
        if (curDetectedMarkers.find(marker->first) != curDetectedMarkers.end()) {
            // not for already found markers
            LOG(debug) << " -> " << marker->first << " already detected..";
            continue;
        }
        if (!hasStaticTransformation(marker->first)) {
            // not for markers where we have no transformation yet
            LOG(debug) << " -> " << marker->first << " has no transformation yet..";
            continue;
        }
        MarkerTransform markerTransformTarget;
        this->getMarkerTransformation(marker->first, markerTransformTarget);
        cv::Size2f targetSize(marker->second.size*1000,marker->second.size*1000);
        // Call the actual method
        LOG(debug) << " -> " << "Calculating (by single markers) relative marker corner for marker " << marker->first;
        std::vector<cv::Point2f> points = calcRelativeMarkerCorners(camera,
                                                                    markerTransformTarget.matrix, 
                                                                    targetSize,
                                                                    transformation);
        corners[marker->first] = points;
    }

    return corners;
}

void
MultiMarkerTracker::calcLikelyMarkers(std::string camera, boost::posix_time::ptime &frameTime, Marker& marker, cv::Size& frameSize) {
    LOG(debug) <<  "Calculating likely markers (body)..";

    std::string bodyName = getBodyNameByMarkerId(marker.id);
    LOG(debug) << " -> Body name: " << bodyName << "\n";

    //this->likelyMutex[camera]->lock();
	//if (staticBody) { // if the body is not static, do not delete old information (it might be better..)
    //	LOG(debug) << " This is a static body. Clearing the likely markers. << "\n";
	//this->cameras[camera].likelyMarkers.clear();
	//}
    //this->cameras[camera].likelyMarkerIt = this->cameras[camera].likelyMarkers.begin(); TODO ok?
    //this->likelyMutex[camera]->unlock();
    
    if (!this->cameras[camera].camMatrix.empty()) {

        cv::Mat transformation = marker.cameraTransformation;
        std::map<int, std::vector<cv::Point2f> > markerCorners;
        MarkerBody mb = getBodyByMarkerId(marker.id);
        if (mb.isStatic){
            markerCorners = calcAllRelativeMarkerCorners(camera, marker.id, transformation);
        }else{

            markerCorners = calcAllRelativeMarkerCorners(camera, marker.id, mb, transformation);
        }
        
        std::map<std::string, std::vector<Marker> > insertionMap;
        for (std::map<int, std::vector<cv::Point2f> >::iterator it = markerCorners.begin(); it != markerCorners.end(); ++it) {
            std::vector<cv::Point2f> p = it->second;
            bool visible = true;
            for (size_t i = 0; i < p.size(); ++i) {
                if (p[i].x < 0 || p[i].y < 0 || p[i].x >= frameSize.width - 1 || p[i].y >= frameSize.height - 1) {
                    visible = false;
                }
            }
            if (visible) {
                Marker m;
                //m.source = "calculated";
                m.source = "[" + bodyName + " ]";
                m.id = it->first;
                m.points = it->second;
                if (m.points.empty()) {
                    continue; //TODO FIXME -> this is a workaround, sometimes no points are there...
                }
                // Estimate quality by image size
                m.quality = std::min(1.0f, ((float)m.getImageSize()) / ( 100000.0f )); // Say a good size is 10000 pixels

                std::string bodyName = getBodyNameByMarkerId(m.id);
                insertionMap[bodyName].push_back(m);
                LOG(debug) << "  -> Added marker " << m.id << " to body " << bodyName << " .";
                LOG(debug) << "      Transformation: " << m.points[0].x << "," << m.points[0].y;
                LOG(debug) << "                      " << m.points[1].x << "," << m.points[1].y;
                LOG(debug) << "                      " << m.points[2].x << "," << m.points[2].y;
                LOG(debug) << "                      " << m.points[3].x << "," << m.points[3].y << "\n";
              // FIX: If we set the iterator to begin here, we will eventually look for the same body  
              //  this->cameras[camera].likelyMarkerIt = this->cameras[camera].likelyMarkers.begin();
            } else {
                LOG(debug) << "  -> Marker " << it->first << " is not visible." << "\n";
                LOG(debug) << "      Transformation: " << it->second[0].x << "," << it->second[0].y;
                LOG(debug) << "                      " << it->second[1].x << "," << it->second[1].y;
                LOG(debug) << "                      " << it->second[2].x << "," << it->second[2].y;
                LOG(debug) << "                      " << it->second[3].x << "," << it->second[3].y << "\n";
            }
        }
        
        // Make the following function calls atomic using recursive lock
        {
            this->cameras[camera].hypothesisManager[frameTime].startAtomicHypothesisOperation();
            
            // Erase formerly calculated likely markers
            this->cameras[camera].hypothesisManager[frameTime].removeHypotheses("[" + bodyName + " ]");
            // Add to likely markers
            this->cameras[camera].hypothesisManager[frameTime].addHypotheses(insertionMap);
            // ..and sort
            this->cameras[camera].hypothesisManager[frameTime].sortHypotheses();
            
            this->cameras[camera].hypothesisManager[frameTime].endAtomicHypothesisOperation();
        }
    }
}

void
MultiMarkerTracker::calcLikelyMarkers(std::string camera, boost::posix_time::ptime &frameTime, cv::Mat& cameraTransformation, cv::Size& frameSize) {
    LOG(debug) <<  "Calculating likely markers (global)..";


    //this->likelyMutex[camera]->lock();
    //this->cameras[camera].likelyMarkers.clear();
    //this->cameras[camera].likelyMarkerIt = this->cameras[camera].likelyMarkers.begin(); TODO ok?
    //this->likelyMutex[camera]->unlock();
    
    if (!this->cameras[camera].camMatrix.empty()) {

        std::map<int, std::vector<cv::Point2f> > markerCorners;
        markerCorners = calcAllMarkerCorners(camera, cameraTransformation);
        
        std::map<std::string, std::vector<Marker> > insertionMap;
        for (std::map<int, std::vector<cv::Point2f> >::iterator it = markerCorners.begin(); it != markerCorners.end(); ++it) {
            std::vector<cv::Point2f> p = it->second;
            bool visible = true;
            for (size_t i = 0; i < p.size(); ++i) {
                if (p[i].x < 0 || p[i].y < 0 || p[i].x >= frameSize.width - 1 || p[i].y >= frameSize.height - 1) {
                    visible = false;
                }
            }
            if (visible) {
                Marker m;
                m.id = it->first;
                m.points = it->second;
                if (m.points.empty()) {
                    continue; //TODO FIXME -> this is a workaround, sometimes no points are there...
                }
                // Estimate quality by image size
                m.quality = std::min(1.0f, ((float)m.getImageSize()) / ( 100000.0f )); // Say a good size is 10000 pixels
                //m.source = "[" + bodyName + " ]";
                m.source = "calculated";

                std::string bodyName = getBodyNameByMarkerId(m.id);
                insertionMap[bodyName].push_back(m);
                LOG(debug) << "  -> Added marker " << m.id << " to body " << bodyName << " .";
                LOG(debug) << "      Transformation: " << m.points[0].x << "," << m.points[0].y;
                LOG(debug) << "                      " << m.points[1].x << "," << m.points[1].y;
                LOG(debug) << "                      " << m.points[2].x << "," << m.points[2].y;
                LOG(debug) << "                      " << m.points[3].x << "," << m.points[3].y << "\n";
              // FIX: If we set the iterator to begin here, we will eventually look for the same body  
              //  this->cameras[camera].likelyMarkerIt = this->cameras[camera].likelyMarkers.begin();
            } else {
                LOG(debug) << "  -> Marker " << it->first << " is not visible." << "\n";
                LOG(debug) << "      Transformation: " << it->second[0].x << "," << it->second[0].y;
                LOG(debug) << "                      " << it->second[1].x << "," << it->second[1].y;
                LOG(debug) << "                      " << it->second[2].x << "," << it->second[2].y;
                LOG(debug) << "                      " << it->second[3].x << "," << it->second[3].y << "\n";
            }
        }
        
        // Make the following function calls atomic using recursive lock
        {
            this->cameras[camera].hypothesisManager[frameTime].startAtomicHypothesisOperation();
            
            // Erase formerly calculated likely markers
            this->cameras[camera].hypothesisManager[frameTime].removeHypotheses("calculated");
            // Add to likely markers
            this->cameras[camera].hypothesisManager[frameTime].addHypotheses(insertionMap);
            // ..and sort
            this->cameras[camera].hypothesisManager[frameTime].sortHypotheses();
            
            this->cameras[camera].hypothesisManager[frameTime].endAtomicHypothesisOperation();
        }
    }
}


// ------------------------------------------------------------------------------------------
// ----------------------- Methods for assignment of markers to bodies ----------------------
// ------------------------------------------------------------------------------------------

MarkerBody
MultiMarkerTracker::getBodyByMarkerId(int id) {
    return this->bodies[this->idBodyMap[id]];
}

std::string
MultiMarkerTracker::getBodyNameByMarkerId(int id) {
    return this->idBodyMap[id];
}

bool 
MultiMarkerTracker::hasStaticTransformation(int id) {
    // Check marker bodies
    if (getBodyNameByMarkerId(id) != "") {
        MarkerBody body = getBodyByMarkerId(id);
        if (body.isStatic) {
            return true;
        }
    // Check learned marker transformations
    } else {
        if (estimatedMarkerTransformations.find(id) != estimatedMarkerTransformations.end()) {
            return true;
        }
    }
    return false;
}

