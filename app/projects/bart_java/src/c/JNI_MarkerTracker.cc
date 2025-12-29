//#include <iostream>
#include "de_citec_clfvr_bart_MarkerTracker.h"
#include <MarkerTracker.h>
#include <iostream>

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

	MarkerTracker* markerTracker = 0;
	cv::Mat bestGlobalTransform;

	void
		callback(MarkerTrackerState state, boost::posix_time::ptime startTime) {

		std::string cam = "default";

		if (state == DONE_PART) {
				std::cout << "Found marker after "
				<< boost::posix_time::time_duration(boost::posix_time::microsec_clock::local_time() - startTime).total_milliseconds()
				<< "ms!" << "\n";

			std::map<int, cv::Mat> markerTransforms;
			std::map<int, cv::Mat> globalTransforms;
			markerTracker->getMarkerTransformations(markerTransforms);
			markerTracker->getCameraTransformations(globalTransforms);

			markerTracker->getBestCameraTransformation(cam, bestGlobalTransform, startTime);

			cout << "cols: " << bestGlobalTransform.cols << "  rows: " << bestGlobalTransform.rows << endl;


			std::vector<int> ids;
			markerTracker->getMarkerIds(ids, startTime);
				std::cout << "Found: ";
				for (unsigned int i = 0; i < ids.size(); ++i) {
					std::cout << ids[i] << " ";
				}
				std::cout << "\n";
			std::string trackerLog = markerTracker->getLog();
		}
		if (state == DONE || state == TIMEOUT) {
			if (state == DONE) {
				std::cout << "Done after "
					<< boost::posix_time::time_duration(boost::posix_time::microsec_clock::local_time() - startTime).total_milliseconds()
					<< "ms. \n";
			}
			else {
				std::cout << "Timeout after "
					<< boost::posix_time::time_duration(boost::posix_time::microsec_clock::local_time() - startTime).total_milliseconds()
					<< "ms. \n";
			}
		}
	}

	/*
	 * Class:     de_citec_clfvr_bart_MarkerTracker
	 * Method:    create
	 * Signature: ()J
	 */
	JNIEXPORT jlong JNICALL Java_de_citec_clfvr_bart_MarkerTracker_create(JNIEnv * env, jobject inst)
	{
		//string configUrl = "C:\\bart\\projects\\bart_java\\config.yaml";
		string configUrl = "config.yaml";
		cout << "MarkerTracker::create" << endl;
		markerTracker = new MarkerTracker(configUrl);
		markerTracker->registerCallback(boost::bind(&callback, _1, _2));
		jlong result = reinterpret_cast<jlong>(markerTracker);

		return result;
	}

	/*
	* Class:     de_citec_clfvr_bart_MarkerTracker
	* Method:    destroy
	* Signature: (J)V
	*/
	JNIEXPORT void JNICALL Java_de_citec_clfvr_bart_MarkerTracker_destroy(JNIEnv * env, jobject inst, jlong ptr)
	{
		cout << "MarkerTracker::destroy" << endl;
	}

	/*
	* Class:     de_citec_clfvr_bart_MarkerTracker
	* Method:    configure
	* Signature: (J)V
	*/
	JNIEXPORT void JNICALL Java_de_citec_clfvr_bart_MarkerTracker_configure(JNIEnv * env, jobject inst, jlong ptr)
	{
		cout << "MarkerTracker::configure" << endl;
	}

	JNIEXPORT void JNICALL Java_de_citec_clfvr_bart_MarkerTracker_processFrame(JNIEnv * env, jobject inst, jlong ptr, jlong frame_p, jdouble scaleDownFactor, jint scales)
	{
		MarkerTracker* mt = reinterpret_cast<MarkerTracker*>(ptr);
		cv::Mat* frame = reinterpret_cast<cv::Mat*>(frame_p);
		//cv::imshow("fenster",*frame);
		//cv::waitKey(1);
		boost::posix_time::ptime frameTime = boost::posix_time::microsec_clock::local_time();
		// nativeObj
		if (mt != 0) {
			cout << "MarkerTracker::processFrame" << endl;
			mt->processFrame(frame->clone(), frameTime, "default", BartTrackingOptions::TIME_LIMITED, 100, scaleDownFactor, scales);
		}
	}

	JNIEXPORT void JNICALL Java_de_citec_clfvr_bart_MarkerTracker_getBestMat
		(JNIEnv * env, jobject inst, jlong ptr, jlong matAddr)
	{
		cv::Mat* mat = (cv::Mat*)matAddr;
		*mat = bestGlobalTransform;
	
	}


#ifdef __cplusplus
}
#endif

