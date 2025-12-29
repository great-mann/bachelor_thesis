#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <string>
#include <conio.h>


// RSB
#include <rsb/Informer.h> 
#include <rsb/Factory.h>
#include <rsb/converter/Converter.h>
#include <rsb/converter/Repository.h>
#include <rsb/converter/ProtocolBufferConverter.h>
#include <rsb/util/EventQueuePushHandler.h>

// RST
#include <rst/vision/Faces.pb.h>
#include <rst/vision/Face.pb.h>
#include <rst/math/Vec2DInt.pb.h>
#include <rst/math/MatrixDouble.pb.h>
#include <rst/geometry/BoundingBox.pb.h>
#include <rst/stochastics/MixtureOfGaussian1D.pb.h>
#include <rst/vision/HeadObjects.pb.h>
#include <rst/vision/HeadObject.pb.h>
#include <rst/converters/opencv/IplImageConverter.h>


#include <rsc/logging/LoggerFactory.h>
// CV
#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <boost/date_time/posix_time/posix_time_types.hpp>

using namespace std;
using namespace cv;
using namespace rsb;
using namespace rsc::logging;

typedef rsb::util::EventQueuePushHandler MatrixHandler;
typedef boost::shared_ptr<MatrixHandler> MatrixHandlerPtr;
typedef rsc::threading::SynchronizedQueue<rsb::EventPtr> MatrixQueue;
typedef boost::shared_ptr<MatrixQueue> MatrixQueuePtr;


static MatrixQueuePtr matrixQueue(new MatrixQueue(1));


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

void printData(boost::shared_ptr<string> e) {
	std::cout << "Received event: " << e.get() << std::endl;
	
	
}

int main(int argc, char **argv) {
	string imageScopeAddress("/img");
	string matrixScopeAddress("/trans");

	if (argc >= 2)
		imageScopeAddress = argv[1];

	if (argc >= 3)
		matrixScopeAddress = argv[2];

	rsb::Factory &factory = rsb::getFactory();

	try {
		rsb::converter::Converter<std::string>::Ptr imageP(new rst::converters::opencv::IplImageConverter());
		rsb::converter::converterRepository<string>()->registerConverter(imageP);
		boost::shared_ptr<rsb::converter::ProtocolBufferConverter<rst::math::MatrixDouble> > matrix_c(
			new rsb::converter::ProtocolBufferConverter<rst::math::MatrixDouble>());
		rsb::converter::converterRepository<string>()->registerConverter(
			matrix_c);
	}
	catch (...) {
		cout << ">> RSB IS WEIRD (converter already registered)" << endl;
	}
	cout << ">> sending on " << imageScopeAddress << endl;
	rsb::Informer<IplImage>::Ptr informer = factory.createInformer<IplImage>(imageScopeAddress);
	rsb::ListenerPtr listener = factory.createListener(matrixScopeAddress);
	cout << "listener created" << endl;
	MatrixHandlerPtr matrixHandler(new MatrixHandler(matrixQueue));


	listener->addHandler(boost::static_pointer_cast<rsb::Handler>(matrixHandler));

	while (true)
	{
		Mat frame = cv::imread("test.png");
		//cv::imshow("test", img);
		int depth = frame.depth();
		int channels = frame.channels();
		IplImage *iplImage = cvCreateImage(cvSize(frame.cols, frame.rows), 8, channels);
		iplImage->widthStep = frame.step;
		iplImage->imageData = (char*)frame.data;
		// warp in shared_ptr
		//Informer<IplImage>::DataPtr img(iplImage);
		boost::shared_ptr<IplImage> img = boost::shared_ptr<IplImage>(iplImage, ReleaseImage());
		// send image to RSB, memory is deallocated through shared_ptr
		cout << ">> boost pointer " << img << endl;
		cvShowImage("test", iplImage);
		informer->publish(img);
		//cout << ">> sended" << endl;
		rsb::EventPtr matrixEvent;


		try {
			matrixEvent = matrixQueue->tryPop();

			assert(matrixEvent->getType() == rsc::runtime::typeName<rst::math::MatrixDouble>());
			boost::shared_ptr<rst::math::MatrixDouble> matrix = boost::static_pointer_cast<
				rst::math::MatrixDouble>(matrixEvent->getData());
			matrix.get()->data().PrintDebugString();

		}
		catch (...) {
			cout << ">> No element" << endl;
		}


		

		if (waitKey(300) >= 0) break;

	}
}

