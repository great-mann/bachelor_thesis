#include "bart_rsb_server.h"

#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <string>

// RSB
#include <rsb/Listener.h>
#include <rsb/Factory.h>
#include <rsb/converter/Converter.h>
#include <rsb/converter/Repository.h>
#include <rsb/converter/ProtocolBufferConverter.h>
#include <rsb/util/EventQueuePushHandler.h>
#include <rsb/EventCollections.h>

//// RST
#include <rst/vision/Faces.pb.h>
#include <rst/vision/Face.pb.h>
#include <rst/math/Vec2DInt.pb.h>
#include <rst/math/MatrixDouble.pb.h>
#include <rst/geometry/BoundingBox.pb.h>
#include <rst/stochastics/MixtureOfGaussian1D.pb.h>
#include <rst/vision/HeadObjects.pb.h>
#include <rst/vision/HeadObject.pb.h>
#include <rst/converters/opencv/IplImageConverter.h>

// RSC
#include <rsc/misc/langutils.h>
#include <rsc/threading/SynchronizedQueue.h>
#include <rsc/misc/SignalWaiter.h>

// CV
#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <boost/date_time/posix_time/posix_time_types.hpp>
//
using namespace std;
using namespace rsc::logging;
using namespace rsb;
using namespace cv;

void printData(boost::shared_ptr<IplImage> e) {
	std::cout << "Received event: "  << std::endl;
}

class ReleaseImage {
public:
	ReleaseImage() {};

	/**
	* Releases an IplImage.
	* @param img The IplImage pointer.
	*/
	void operator()(IplImage* img) {
		cout << "test10" << endl;
		if (img == 0) return;
		cout << "test11" << endl;
		cvReleaseImage(&img);
		cout << "test12" << endl;
		img = 0;
	};
};

BartRSBServer::BartRSBServer(string in, string out, bool invert, bool debug){
	markerTracker = new MarkerTracker(string("config.yaml"));
	markerTracker->registerCallback(boost::bind(&BartRSBServer::callback, this, _1, _2));

	this->invert = invert;
	this->debug = debug;
	//rsc::misc::initSignalWaiter();

	imageQueue = ImageQueuePtr(new ImageQueue(5));
	imageHandler = ImageHandlerPtr(new ImageHandler(imageQueue));

	try {
		rsb::converter::Converter<std::string>::Ptr image_c(new rst::converters::opencv::IplImageConverter());
		rsb::converter::converterRepository<string>()->registerConverter(image_c);
		
		boost::shared_ptr<rsb::converter::ProtocolBufferConverter<rst::math::MatrixDouble> > matrix_c(
			new rsb::converter::ProtocolBufferConverter<rst::math::MatrixDouble>());
		rsb::converter::converterRepository<string>()->registerConverter(
			matrix_c);

	}
	catch (...) {
		cout << ">> RSB IS WEIRD (converter already registered)" << endl;
	}
	rsb::Factory &factory = rsb::getFactory();
	cout << "listening on " << in << endl;
	listener = factory.createListener(rsb::Scope(in));
	listener->addHandler(boost::static_pointer_cast<rsb::Handler>(imageHandler));
	//listener->addHandler(rsb::HandlerPtr(new rsb::DataFunctionHandler<IplImage>(&printData)));
	informer = factory.createInformer<rst::math::MatrixDouble>(out);



	//rsc::misc::suggestedExitCode(rsc::misc::waitForSignal());
	if (debug)
		namedWindow("debug image", WINDOW_NORMAL);

	while (true) {
		cv::waitKey(300);
		rsb::EventPtr imageEvent;
		imageEvent = imageQueue->pop();

		assert(imageEvent->getType() == rsc::runtime::typeName<IplImage>());
		boost::shared_ptr<IplImage> image = boost::static_pointer_cast<
			IplImage>(imageEvent->getData());
		if (debug)
			cvShowImage("debug image", image.get());

		//Mat img = cv::imread("test.png");
		boost::posix_time::ptime frameTime = boost::posix_time::microsec_clock::local_time();


		markerTracker->processFrame(cv::Mat(image.get()).clone(), frameTime, "default", BartTrackingOptions::TIME_LIMITED);

		/*Mat* showImg = new Mat();
		getImage(showImg);*/
		//imshow("test",*showImg);
		cv::waitKey(3);
	}
}


void BartRSBServer::callback(MarkerTrackerState state, boost::posix_time::ptime startTime) {
	std::string cam = "default";

	if (state != DONE) {
		std::vector<int> ids;
		markerTracker->getMarkerIds(ids, startTime);
		if (!ids.empty() && debug) {
			 std::cout << "Found " << ids.size() << " markers: " << std::endl;
			  for (unsigned int i = 0; i < ids.size(); ++i) {
			      std::cout << "   Marker " << ids[i] << std::endl;
			  }
		}

		// Output best camera transformation
		cv::Mat bestGlobalTransform;
		markerTracker->getBestCameraTransformation(cam, bestGlobalTransform, startTime);
		

		if (invert)
			bestGlobalTransform = bestGlobalTransform.inv();
		cout << "Transform = " << endl << " " << bestGlobalTransform << endl << endl;
		rst::math::MatrixDouble* mat = new rst::math::MatrixDouble();
		rst::math::MatrixDouble::Data* data = new rst::math::MatrixDouble::Data();
		rst::math::MatrixDouble::Size* size = new rst::math::MatrixDouble::Size();
		size->set_m(4);
		size->set_n(4);
		
		bestGlobalTransform.convertTo(bestGlobalTransform, CV_64F, 1, 0);

		for (int i = 0; i < bestGlobalTransform.rows; i++)
			for (int j = 0; j < bestGlobalTransform.cols; j++)
				data->add_value(bestGlobalTransform.at<double>(i, j));
		if (debug)
			data->PrintDebugString();
		mat->set_allocated_data(data);
		mat->set_allocated_size(size);
		//rsb::Informer<string>::DataPtr result(new string("test"));
		boost::shared_ptr<rst::math::MatrixDouble> result(mat);
		this->informer->publish(result);
	}
}



int main(int argc, char **argv) {
	if ((argc == 2) && (!strcmp(argv[1], "-h"))) {
		cout
			<< "bart_rsb_server INPUTSCOPE OUTPUTSCOPE [options]\n\n"
			<< " Options:\n"
			<< "  invert output matrix             -i \n"
			<< "                                   --invert \n"
		    << "  show debug infos                 -d\n"
			<< "                                   --debug \n";
		return (0);
	}
	bool debug = false;
	bool invert = false;
	if (argc == 3)
		BartRSBServer(string(argv[1]), string(argv[2]), false, false);
	if ((argc >= 4) && ((!strcmp(argv[3], "-i")) || (!strcmp(argv[3], "--invert"))))
		invert = true;
	if ((argc >= 4) && ((!strcmp(argv[3], "-d")) || (!strcmp(argv[3], "--debug"))))
		debug = true;
	if ((argc == 5) && ((!strcmp(argv[4], "-i")) || (!strcmp(argv[4], "--invert"))))
		invert = true;
	if ((argc == 5) && ((!strcmp(argv[4], "-d")) || (!strcmp(argv[4], "--debug"))))
		debug = true;
	if (argc >= 4)
		BartRSBServer(string(argv[1]), string(argv[2]), invert, debug);
}

