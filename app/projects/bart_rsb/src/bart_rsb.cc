#include "bart_rsb.h"

#include <bart/MarkerTracker.h>


#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>


#include <boost/thread.hpp>

#include <rsb/Handler.h>
#include <rsb/Listener.h>
#include <rsb/Informer.h>
#include <rsb/Factory.h>
#include <rsb/converter/Converter.h>
#include <rsb/converter/Repository.h>
#include <rsb/converter/ProtocolBufferConverter.h>

#include <rst/converters/opencv/IplImageConverter.h>


#include <TransformEvent.pb.h>
#include <Transform.pb.h>


#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <string>
#include <queue>
#include <limits>



using namespace std;
using namespace rsb;
using namespace rsb::patterns;
using namespace cv;

boost::posix_time::ptime startTime;
boost::mutex mutex;
MarkerTracker* markerTracker;

cv::Mat outMat;
size_t numberFrames = 0;
size_t numberDetected = 0;

// Create an informer that is capable of sending events containing
// string data on the scope "/example/informer".
rsb::Informer<string>::Ptr idsInformer;
rsb::Informer<TransformEventT>::Ptr transformationInformer;
rsb::Informer<IplImage>::Ptr imageInformer;



class ReleaseImage {
public:
	ReleaseImage() {};

	/**
	* Releases an IplImage.
	* @param img The IplImage pointer.
	*/
	void operator()(IplImage* img) {
		if (img == 0) return;
		cvReleaseImage(&img);
		img = 0;
	};
};

cv::Mat testImage;



void calculateOrientation(cv::Mat& a, OrientationT* q) {
    
    std::cout << a << std::endl;

    float trace = a.at<float>(0, 0) + a.at<float>(1, 1) + a.at<float>(2, 2); 
    if (trace > 0) {
        float s = 0.5f / sqrt(trace + 1.0f);
        q->set_w(0.25f / s);
        q->set_x((a.at<float>(2, 1) - a.at<float>(1, 2)) * s);
        q->set_y((a.at<float>(0, 2) - a.at<float>(2, 0)) * s);
        q->set_z((a.at<float>(1, 0) - a.at<float>(0, 1)) * s);
    } else {
        if (a.at<float>(0, 0) > a.at<float>(1, 1) && a.at<float>(0, 0) > a.at<float>(2, 2)) {
            float s = 2.0f * sqrt(1.0f + a.at<float>(0, 0) - a.at<float>(1, 1) - a.at<float>(2, 2));
            q->set_w((a.at<float>(2, 1) - a.at<float>(1, 2)) / s);
            q->set_x(0.25f * s);
            q->set_y((a.at<float>(0, 1) + a.at<float>(1, 0)) / s);
            q->set_z((a.at<float>(0, 2) + a.at<float>(2, 0)) / s);
        } else if (a.at<float>(1, 1) > a.at<float>(2, 2)) {
            float s = 2.0f * sqrt(1.0f + a.at<float>(1, 1) - a.at<float>(0, 0) - a.at<float>(2, 2));
            q->set_w((a.at<float>(0, 2) - a.at<float>(2, 0)) / s);
            q->set_x((a.at<float>(0, 1) + a.at<float>(1, 0)) / s);
            q->set_y(0.25f * s);
            q->set_z((a.at<float>(1, 2) + a.at<float>(2, 1)) / s);
        } else {
            float s = 2.0f * sqrt(1.0f + a.at<float>(2, 2) - a.at<float>(0, 0) - a.at<float>(1, 1));
            q->set_w((a.at<float>(1, 0) - a.at<float>(0, 1)) / s);
            q->set_x((a.at<float>(0, 2) + a.at<float>(2, 0)) / s);
            q->set_y((a.at<float>(1, 2) + a.at<float>(2, 1)) / s);
            q->set_z(0.25f * s);
        }
    }
    
}

void calculatePosition(cv::Mat& a, PositionT* p) {
    
    p->set_x(a.at<float>(3,0));
    p->set_y(a.at<float>(3,1));
    p->set_z(a.at<float>(3,2));
}


void 
    callback(MarkerTrackerState state, boost::posix_time::ptime startTime) {
		
		if (state == DONE_PART) {
            std::cout << "Found a marker after " 
                          << boost::posix_time::time_duration(boost::posix_time::microsec_clock::local_time() - startTime).total_milliseconds()
                          << "ms!" << std::endl;
                
            std::map<int, cv::Mat> markerTransforms;
            std::map<int, cv::Mat> globalTransforms;
            markerTracker->getMarkerTransformations(markerTransforms);
            markerTracker->getCameraTransformations(globalTransforms);
            std::vector<int> ids;
            markerTracker->getMarkerIds(ids, startTime);
            std::string trackerLog = markerTracker->getLog();
        }
        
		if (state == DONE || state == TIMEOUT) {
			mutex.lock();
			boost::this_thread::disable_interruption disableInterruption;
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

			// Check if the marker was found
			++numberFrames;
			std::vector<int> ids;
			markerTracker->getMarkerIds(ids, startTime);
			std::map<int, cv::Mat> markerTransformations;
			markerTracker->getMarkerTransformations(markerTransformations);
			if (!ids.empty()) {
				++numberDetected;
			}

			cv::Mat markerImg = markerTracker->getImage("default", startTime);
			testImage = markerImg.clone();

			std::stringstream idSS;
			std::stringstream trSS;
			std::cout << "--- Detected markers in " << numberDetected << " of " << numberFrames << " frames." << "\n";
            
            TransformEventT *tEvent = new TransformEventT(); // setupt the transform event for message passing
			for (unsigned int i = 0; i < ids.size(); ++i) {
				//				std::cout << "   -> Marker " << ids[i] << "\n";
				//                std::cout << "        Matrix: " << markerTransformations[ids[i]] << "\n";
				//                cv::Vec3f t;
				//                cv::Vec4f r;
				//                GeometryHelpers::decomposeMat(markerTransformations[ids[i]], t, r);
				//                std::cout << "        Translation: " << t << "\n";
				//                std::cout << "        Orientation: " << r << "\n";

				// Fill informers
				idSS << ids[i];
				if (!markerTransformations[ids[i]].empty()) {
					trSS << ids[i] << " ";
					for (unsigned int j = 0; j < 16; ++j) {
						trSS << markerTransformations[ids[i]].at<float>(j);
						if (j < 15) {
							trSS << ",";
						}
					}
					if (i < ids.size() - 1) {
						trSS << " ";
					}
				}
				if (i < ids.size() - 1) {
					idSS << " ";
				}
                
                // -- Fill transform message --
                TransformT *tt = tEvent->add_transforms();
                stringstream idStr;
                idStr << ids[i];
                tt->set_id(idStr.str());
                tt->set_framereferenceid("BART");
                tt->set_certainty(1.0);
                // Set orientation and position
                if (!markerTransformations[ids[i]].empty()) { 
                    OrientationT *ot = new OrientationT();
                    calculateOrientation(markerTransformations[ids[i]], ot);
                    PositionT *pt = new PositionT();
                    calculatePosition(markerTransformations[ids[i]], pt);
                    tt->set_allocated_orientation(ot);
                    tt->set_allocated_position(pt);
                    // Set matrix
                    MatrixT *mt = new MatrixT();
                    for (unsigned int k = 0; k < 16; ++k) {
                        mt->add_value(markerTransformations[ids[i]].at<float>(k));
                    }
                    tt->set_allocated_matrix(mt);
                }
			}
            rsb::Informer<TransformEventT>::DataPtr trInf(tEvent);
			transformationInformer->publish(trInf);
            

			// convert marker image to IplImage*
			if (!markerImg.empty()) {
				std::cout << "Output" << std::endl;
				int depth = markerImg.depth();
				int channels = markerImg.channels();
				IplImage *iplImage = cvCreateImage(cv::Size(markerImg.cols, markerImg.rows), 8, channels);
				iplImage->widthStep = markerImg.step;
				iplImage->imageData = (char*)markerImg.data;

				// warp in shared_ptr
				boost::shared_ptr<IplImage> img = boost::shared_ptr<IplImage>(iplImage, ReleaseImage());
				imageInformer->publish(img);
			}



			 // Send data
			rsb::Informer<string>::DataPtr idInf(new string(idSS.str()));
			idsInformer->publish(idInf);
			

            mutex.unlock();
        }
    }





int main(int argc, char* argv[]) {
	
	cv::namedWindow("Output");
    
    string configURL;
    if (argc > 1) {
        std::cout << "Trying to use first argument as config file..." << std::endl;
        configURL = argv[1];
    } else {
        cout << "No config file specified! Cannot calculate 3D positions! " << endl;
    }


	// First get a factory instance that is used to create RSB domain
    // objects.
    rsb::Factory& factory = getFactory();

	// Register the converters
	rsb::converter::Converter<std::string>::Ptr imageP(new rst::converters::opencv::IplImageConverter());
	rsb::converter::converterRepository<string>()->registerConverter(imageP);
	boost::shared_ptr< rsb::converter::ProtocolBufferConverter<TransformEventT> >
        transformEventConverter(new rsb::converter::ProtocolBufferConverter<TransformEventT>());
    rsb::converter::converterRepository<std::string>()->registerConverter(transformEventConverter);
    boost::shared_ptr< rsb::converter::ProtocolBufferConverter<TransformT> >
        transformConverter(new rsb::converter::ProtocolBufferConverter<TransformT>());
    rsb::converter::converterRepository<std::string>()->registerConverter(transformConverter);


	// Create informers
    idsInformer = factory.createInformer<string> ("/bart/markerIds");
	transformationInformer = factory.createInformer<TransformEventT> ("/bart/markerTransformations");
	imageInformer = factory.createInformer<IplImage>("/bart/markerImage");
    
	// Marker Tracker
    if (configURL != "") {
        markerTracker = new MarkerTracker(configURL);
    } else {
        markerTracker = new MarkerTracker();
    }
	markerTracker->registerCallback(boost::bind(&callback, _1, _2));



	// Video capture for internal input
	cv::VideoCapture capture;
	capture = cv::VideoCapture(0);
	if (!capture.isOpened()) {
		std::cerr << "Could not open capture device!" << std::endl;
	}

    // As events are received asynchronously we have to wait here for
    // them.
	double downScaleFactor = 0.5f;
	int scales = 2;
	cv::Mat frame;
	size_t frameNumber = 0;
	while (capture.read(frame)) {
		cout << "  Frame " << frameNumber++ << std::endl;
		if (!testImage.empty()) {
			//cv::imshow("Output", testImage);
			cv::waitKey(1);
		}
		
		
		boost::posix_time::ptime frameTime = boost::posix_time::microsec_clock::local_time();

		markerTracker->processFrame(frame.clone(), frameTime, "default", BartTrackingOptions::TIME_LIMITED_PRIOR_AND_ONE_BG_SEARCH, 50, downScaleFactor, scales);

	}

    return EXIT_SUCCESS;
}
