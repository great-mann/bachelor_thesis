#include "bart_iio.h"

#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <string>

#include <InstantIO/OutSlot.h>
#include <InstantIO/FieldAccessor.h>
#include <boost/date_time/posix_time/posix_time_types.hpp>

//#include <qrencode.h>




namespace InstantIO {

    // Node Type 
    NodeType bart_iio::type_(
            typeName_,
            &create,
            shortDescription_,
            longDescription_,
            author_,
            fields_,
            sizeof (fields_) / sizeof (Field));

    const char *bart_iio::typeName_ =
            "bart_iio";

    const char *bart_iio::shortDescription_ =
            "Inputs images and outputs marker positions on it.";

    const char *bart_iio::longDescription_ =
            "Inputs images and outputs marker positions on it.";

    const char *bart_iio::author_ =
            "Patrick Renner";

    // ###ADDCODE###
    // Add additional fields here
    Field bart_iio::fields_[] = {
        Field(
        "configUrl", // Name
        "The url of the config file", // description
        "", // default value
        FieldAccessor<bart_iio, std::string>(&bart_iio::setConfigUrl,
        &bart_iio::getConfigUrl)
        ),
        Field(
        "markerCoordinateSystemIsXZ", // Name
        "If true, then the coordinate system of the markers is set to XZ instead of XY", // description
        "false", // default value
        FieldAccessor<bart_iio, bool>(&bart_iio::setMarkerCoordinateSystemIsXZ,
        &bart_iio::getMarkerCoordinateSystemIsXZ)
        ),
        Field(
        "useInternalInput", // Name
        "If true, a capture device is opened internally", // description
        "false", // default value
        FieldAccessor<bart_iio, bool>(&bart_iio::setUseInternalInput,
        &bart_iio::getUseInternalInput)
        ),
        Field(
        "flipImage", // Name
        "If true, input images are flipped", // description
        "false", // default value
        FieldAccessor<bart_iio, bool>(&bart_iio::setFlipImage,
        &bart_iio::getFlipImage)
        ),
        Field(
        "InternalInputCameraNumber", // Name
        "The number of the internal camera to be used", // description
        "0", // default value
        FieldAccessor<bart_iio, int>(&bart_iio::setInternalInputCameraNumber,
        &bart_iio::getInternalInputCameraNumber)
        ),
		Field(
        "internalWidth", // Name
        "Desired width used for the internal capture", // description
        "320", // default value
        FieldAccessor<bart_iio, long>(&bart_iio::setInternalWidth,
        &bart_iio::getInternalWidth)
        ),
		Field(
        "internalHeight", // Name
        "Desired height used for the internal capture", // description
        "240", // default value
        FieldAccessor<bart_iio, long>(&bart_iio::setInternalHeight,
        &bart_iio::getInternalHeight)
		),
		Field(
		"inputFormat", // Name
		"Format of the input image", // description
		"bgr", // default value
		FieldAccessor<bart_iio, std::string>(&bart_iio::setInputFormat,
		&bart_iio::getInputFormat)
		)
    };

    //----------------------------------------------------------------------

    bart_iio::bart_iio() :
    ThreadedNode(),
    configUrl(""),
    useInternalInput(false),
	width(getInternalWidth()),
	height(getInternalHeight()),
	internalInputCameraNumber( 0 ),
	markerCoordinateSystemIsXZ( false ),
    currentCamera ( "default" ),
    flipImage(false),
	inputFormat("bgr")
	{
        // Add external route
        addExternalRoute("*", "{NamespaceLabel}/{SlotLabel}");
    }

    //----------------------------------------------------------------------

    bart_iio::~bart_iio() {
        ;
    }

    //----------------------------------------------------------------------

    Node *bart_iio::create() {
        return new bart_iio;
    }

    //----------------------------------------------------------------------

    NodeType *bart_iio::type() const {
        return &type_;
    }

    //----------------------------------------------------------------------

    void bart_iio::initialize() {
        // handle state and namespace updates
        Node::initialize();

        addLog("bart_iio: Run initialize");

        cameraId = new InSlot<std::string>("Camera Id");
        assert(cameraId != 0);
        cameraId->addListener(*this);
        addInSlot("cameraId", cameraId);
        
        inputImage = new InSlot<ImagePtr>("Input Image");
        assert(inputImage != 0);
        inputImage->addListener(*this);
        addInSlot("inputImage", inputImage);

		frameNumber = new InSlot<long>("Input Image frame number");
        assert(frameNumber != 0);
        frameNumber->addListener(*this);
        addInSlot("frameNumber", frameNumber);

        markerMatrizes = new OutSlot<MFMatrix4fPtr>("Marker positions", MFMatrix4fPtr());
        assert(markerMatrizes != 0);
        markerMatrizes->addListener(*this);
        addOutSlot("markerMatrizes", markerMatrizes);
        
        globalMarkerMatrizes = new OutSlot<MFMatrix4fPtr>("Global Marker matrices", MFMatrix4fPtr());
        assert(globalMarkerMatrizes != 0);
        globalMarkerMatrizes->addListener(*this);
        addOutSlot("globalMarkerMatrizes", globalMarkerMatrizes);
        
        globalCameraTransform = new OutSlot<Matrix4f>("Best global camera transformation", Matrix4f());
        assert(globalCameraTransform != 0);
        globalCameraTransform->addListener(*this);
        addOutSlot("globalCameraTransform", globalCameraTransform);

        markerIds = new OutSlot<MFInt32Ptr>("Marker ids", MFInt32Ptr());
        assert(markerIds != 0);
        markerIds->addListener(*this);
        addOutSlot("markerIds", markerIds);
        
        dynamicBodyNames = new OutSlot<MFStringPtr>("dynamicBodyNames", MFStringPtr());
        assert(dynamicBodyNames != 0);
        dynamicBodyNames->addListener(*this);
        addOutSlot("dynamicBodyNames", dynamicBodyNames);
        
        dynamicBodyMatrices = new OutSlot<MFMatrix4fPtr>("dynamicBodyMatrices", MFMatrix4fPtr());
        assert(dynamicBodyMatrices != 0);
        dynamicBodyMatrices->addListener(*this);
        addOutSlot("dynamicBodyMatrices", dynamicBodyMatrices);

        outputImage = new OutSlot<ImagePtr>("Output image");
        assert(outputImage != 0);
        outputImage->addListener(*this);
        addOutSlot("outputImage", outputImage);
        
        outputMarkerImage = new OutSlot<ImagePtr>("Output image with markers");
        assert(outputMarkerImage != 0);
        outputMarkerImage->addListener(*this);
        addOutSlot("outputMarkerImage", outputMarkerImage);
        
		fieldOfView = new OutSlot<float>("Field of view (from camera calibration)");
        assert(fieldOfView != 0);
        fieldOfView->addListener(*this);
        addOutSlot("fieldOfView", fieldOfView);

		markerFrameNumber = new OutSlot<long>("Output frame number, maps frameNumber to corresponding output.");
        assert(markerFrameNumber != 0);
        markerFrameNumber->addListener(*this);
        addOutSlot("markerFrameNumber", markerFrameNumber);
        
        // QR
        qrString = new InSlot<std::string>("String to be converted to a QR code");
        assert(qrString != 0);
        qrString->addListener(*this);
        addInSlot("qrString", qrString);
        
        qrImage = new OutSlot<ImagePtr>("Output image: The QR-code generated from qrString");
        assert(qrImage != 0);
        qrImage->addListener(*this);
        addOutSlot("qrImage", qrImage);
        

        // Initialize marker tracker
		markerTracker = new MarkerTracker(configUrl);
        markerTracker->registerCallback(boost::bind(&bart_iio::callback, this, _1, _2));
        
        addLog("bart_iio: Initialization Done!");

    }

    //----------------------------------------------------------------------

    void bart_iio::shutdown() {
        Node::shutdown();

        addLog("bart_iio: Run shutdown");

        if (capture.isOpened()) {
            capture.release();
        }
        
        if (cameraId != 0) {
            removeInSlot("cameraId", cameraId);
            delete cameraId;
            cameraId = 0;
        }
        if (inputImage != 0) {
            removeInSlot("inputImage", inputImage);
            delete inputImage;
            inputImage = 0;
        }
		if (frameNumber != 0) {
            removeInSlot("frameNumber", frameNumber);
            delete frameNumber;
            frameNumber = 0;
        }
        if (markerMatrizes != 0) {
            removeOutSlot("markerMatrizes", markerMatrizes);
            delete markerMatrizes;
            markerMatrizes = 0;
        }
        if (globalMarkerMatrizes != 0) {
            removeOutSlot("globalMarkerMatrizes", globalMarkerMatrizes);
            delete globalMarkerMatrizes;
            globalMarkerMatrizes = 0;
        }
        if (globalCameraTransform != 0) {
            removeOutSlot("globalCameraTransform", globalCameraTransform);
            delete globalCameraTransform;
            globalCameraTransform = 0;
        }
        if (markerIds != 0) {
            removeOutSlot("markerIds", markerIds);
            delete markerIds;
            markerIds = 0;
        }
        if (dynamicBodyNames != 0) {
            removeOutSlot("dynamicBodyNames", dynamicBodyNames);
            delete dynamicBodyNames;
            dynamicBodyNames = 0;
        }
        if (dynamicBodyMatrices != 0) {
            removeOutSlot("dynamicBodyMatrices", dynamicBodyMatrices);
            delete dynamicBodyMatrices;
            dynamicBodyMatrices = 0;
        }
        if (outputImage != 0) {
            removeOutSlot("outputImage", outputImage);
            delete outputImage;
            outputImage = 0;
        }
        if (outputMarkerImage != 0) {
            removeOutSlot("outputMarkerImage", outputMarkerImage);
            delete outputMarkerImage;
            outputMarkerImage = 0;
        }
        if (fieldOfView != 0) {
            removeOutSlot("fieldOfView", fieldOfView);
            delete fieldOfView;
            fieldOfView = 0;
        }
		if (markerFrameNumber != 0) {
            removeOutSlot("markerFrameNumber", markerFrameNumber);
            delete markerFrameNumber;
            markerFrameNumber = 0;
        }
        
        if (qrString != 0) {
            removeInSlot("qrString", qrString);
            delete qrString;
            qrString = 0;
        }
        if (qrImage != 0) {
            removeOutSlot("qrImage", qrImage);
            delete qrImage;
            qrImage = 0;
        }
        
        if (markerTracker != 0) {
            delete markerTracker;
        }
    }

    // Thread method which gets automatically started as soon as a slot is
    // connected

    int bart_iio::processData() {
        bool deviceOpen = true;

        addLog("bart_iio: Run processData");

        if (deviceOpen)
            setState(NODE_RUNNING);
        else {
            setState(NODE_ERROR);
            return -1;
        }

        // Prepare OpenCV video capture
        if (useInternalInput) {
            addLog("Opening internal camera...");
			//if (!capture.open("MarkerTest3.asf"))
			if (!capture.open(internalInputCameraNumber))
                addLog("...opening camera failed!");
        }
		//capture.set(CV_CAP_PROP_FOURCC , CV_FOURCC('R','L','E','8'));
        capture.set(CV_CAP_PROP_FRAME_WIDTH, width);
        capture.set(CV_CAP_PROP_FRAME_HEIGHT, height);
		capture.set(CV_CAP_PROP_FPS , 30);

		long internalFrameNumber = 0;

        // Important: you need to call waitThread in every loop
        // time is in millisecond
        while (waitThread(1)) {
            
            // Firstly handle QR stuff
            //if (!qrString->empty()) {
            //    std::string qrStr = qrString->pop();
            //    QRcode* qrCode = QRcode_encodeString(qrStr.c_str(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
            //    unsigned char* qrData = qrCode->data;
            //    ImagePtr qrPtr;
            //    qrPtr->setParameters(qrCode->width, qrCode->width, Image::FMT_Y800);
            //    char* ptrData = qrPtr->getBuffer();
            //    for (size_t i = 0; i < qrCode->width*qrCode->width; ++i) {
            //        if (((qrData[i] >> 0) & 1)) { // Get least significant bit: 1->black, 0->white
            //            ptrData[i] = 0;
            //        } else {
            //            ptrData[i] = 255;
            //        }
            //    }
            //    qrImage->push(qrPtr);
            //}
            

			if (!cameraId->empty()) {
                currentCamera = cameraId->pop();
            }
            
            if (!frameNumber->empty()) {
                frameNumberMutex.lock();
				frameNumbers[currentCamera].push(frameNumber->pop());
                frameNumberMutex.unlock();
			}

            // Try to get an image...
			cv::Mat mat = cv::Mat();
            if (!useInternalInput && !inputImage->empty()) {
                addLog("Received new Image...");
				++internalFrameNumber;
	            ImagePtr imgPtr;
                imgPtr = inputImage->pop();
				if( imgPtr->getSize() > 0 ) {
					//mat = cv::Mat(img->getWidth(), img->getHeight(), CV_32FC3, img->getBuffer());
					IplImage* iplIn;
					if (inputFormat == "rgba") {
						iplIn = cvCreateImage(cvSize(imgPtr->getWidth(), imgPtr->getHeight()), IPL_DEPTH_8U, 4);
						iplIn->imageData = imgPtr->getBuffer();
						mat = cv::Mat(iplIn, true).clone();
						cvReleaseImage(&iplIn);
						// Convert Image RGB to Mat BGR
						cv::cvtColor(mat, mat, CV_RGBA2BGR);
					} else if (inputFormat == "uint16") {
						iplIn = cvCreateImage(cvSize(imgPtr->getWidth(), imgPtr->getHeight()), IPL_DEPTH_16U, 1);
						iplIn->imageData = imgPtr->getBuffer();
						cv::Mat matTmp = cv::Mat(iplIn, true).clone();
						//cv::imshow("mat in 16 bit", matTmp);
						//std::cout << "Type of mat = " << matTmp.type() << " " << matTmp.channels() << std::endl;
						//matTmp = matTmp / 256;
						
						//mat = cv::Mat(cv::Size(imgPtr->getWidth(), imgPtr->getHeight()), CV_8UC1, matTmp.data);
						
						matTmp.convertTo(mat, CV_8UC1, 1.0f/256.0f); // , 
						//cv::imshow("mat in 8 bit", mat);
						cv::waitKey(1);
						cvReleaseImage(&iplIn);
						// Convert Image GRAY to Mat BGR
						cv::cvtColor(mat, mat, CV_GRAY2BGR);
					}
					else {
						iplIn = cvCreateImage(cvSize(imgPtr->getWidth(), imgPtr->getHeight()), IPL_DEPTH_8U, 3);
						iplIn->imageData = imgPtr->getBuffer();
						mat = cv::Mat(iplIn, true).clone();
						cvReleaseImage(&iplIn);
					}
					

					//cv::flip(mat, mat, 0);
                    if (this->flipImage) {
                        cv::Mat flipped;
                        cv::flip(mat, flipped,1 ); // 1
						//cv::flip(flipped, mat, 2);
                        mat = flipped;
                    }
				}
            } else if (capture.isOpened()) {
                //addLog("Trying to capture image...");
                capture >> mat;
				++internalFrameNumber;
                //std::cout << "Pushing frame number " << internalFrameNumber << "." << std::endl;
                frameNumberMutex.lock();
				frameNumbers[currentCamera].push( internalFrameNumber );
                frameNumberMutex.unlock();
            }

            // Start processing if there is a valid mat
            if (!mat.empty()) {
		        // Get field of view
			    fieldOfView->push(markerTracker->getCameraFieldOfView(currentCamera, cv::Size(mat.cols, mat.rows)));
                // Get tracker data
		
				//markerTracker->processFrame(mat.clone(), downScaleFactor, useLastFrameAsPrior, scales);
                startTime = boost::posix_time::microsec_clock::local_time();
                
                markerTracker->processFrame(mat.clone(), startTime, currentCamera,  TIME_LIMITED, 25,  0.5f, 2.0f);

				std::stringstream ss;
				//ss << "Scales: " << scales << std::endl;
				//ss << "Marker Size: " << defaultMarkerSize << std::endl;
                addLog(ss.str());
                //std::cerr << ss.str() << std::endl;
                
				// Output image
				//ImagePtr ptr;
				//IplImage newIpl;
				////if (useInternalInput) {
				//	cv::flip(mat, mat, 0);
				////}
				//newIpl = IplImage(mat.clone());
				//ptr->setParameters(newIpl.width, newIpl.height, Image::FMT_BGR24);
				//memcpy(ptr->getBuffer(), newIpl.imageData, newIpl.imageSize);
				//outputImage->push(ptr);
                
            }
        }

        // Thread finised
        setState(NODE_SLEEPING);

        //###ADDCODE### 
        //close device

        addLog("Foo: finish processData");

        return 0;
    }
    
    
    void 
    bart_iio::callback(MarkerTrackerState state, boost::posix_time::ptime startTime) {
        
        Matrix4f rotationMatrix = Matrix4f();
        if( markerCoordinateSystemIsXZ )
            rotationMatrix.setTranslationAxisAngle( Vec3f(0,0,0), Vec3f(1,0,0), -1.57079632 );
        
        if (state == DONE_PART || state == DONE) {
           // std::cout << "Found a marker after " 
           //               << boost::posix_time::time_duration(boost::posix_time::microsec_clock::local_time() - startTime).total_milliseconds()
           //               << "ms!" << std::endl;
                
            std::map<int, cv::Mat> markerTransforms;
            std::map<int, cv::Mat> globalTransforms;
            markerTracker->getMarkerTransformations(markerTransforms);
            markerTracker->getCameraTransformations(globalTransforms);
            
            std::vector<int> ids;
            markerTracker->getMarkerIds(ids, startTime);
            std::string trackerLog = markerTracker->getLog();
            if (!trackerLog.empty()) {
                addLog(trackerLog);
            }

            // Output transformations
            MFMatrix4fPtr matrizes;
            MFMatrix4fPtr globalMatrizes;

            matrizes->clear();
            for (std::map<int,cv::Mat>::iterator t = markerTransforms.begin(); t!= markerTransforms.end(); ++t) {
                Matrix4f matrix = Matrix4f((float*) t->second.data, true);
                matrix.mult( rotationMatrix );
                matrizes->push_back(matrix);
            }

            globalMatrizes->clear();
            for (std::map<int,cv::Mat>::iterator g = globalTransforms.begin(); g!= globalTransforms.end(); ++g) {

                Matrix4f matrix = Matrix4f((float*) g->second.data, true);
                matrix.mult( rotationMatrix );
                globalMatrizes->push_back(matrix);
            }
            

            
            // Output ids
            MFInt32Ptr idsPtr;
            idsPtr->clear();
            for (unsigned int i = 0; i < ids.size(); ++i) {
                //std::cout << " - Found marker " << ids[i] << std::endl; 
                idsPtr->push_back(ids[i]);
            }
            
                
            markerIds->push(idsPtr);
            markerMatrizes->push(matrizes); 
            globalMarkerMatrizes->push(globalMatrizes);
        }
        
        
        if (state != DONE_PART) {
            std::vector<int> ids;
            markerTracker->getMarkerIds(ids, startTime);
            if (!ids.empty()) {
              // std::cout << "Found " << ids.size() << " markers: " << std::endl;
              //  for (unsigned int i = 0; i < ids.size(); ++i) {
              //      std::cout << "   Marker " << ids[i] << std::endl;
              //  }
            }
            
            
     //       std::map<int, cv::Mat> learnedTransformations = markerTracker->getSingleMarkerTransformations();
     //       if (!learnedTransformations.empty()) {
     //           std::cout << "Estimated transformations: " << std::endl;
     //           std::map<int, cv::Mat>::iterator ltIt = learnedTransformations.begin();
     //           for (; ltIt != learnedTransformations.end(); ++ltIt) {
     //               cv::Mat m = ltIt->second;
     //               std::cout << "  Marker " << ltIt->first << std::endl;
     //               std::cout << "    Mat: " << m << std::endl;
     //               cv::Vec3f t;
     //               cv::Vec4f r;
     //               GeometryHelpers::decomposeMat(m, t, r);
					//std::cout << "    Trans: " << t[0] << "," << t[1] << "," << t[2] << std::endl;
     //               std::cout << "    Rot: " << r[0] << "," << r[1] << "," << r[2] << "," << r[3] << std::endl;
     //               cv::Mat m2 = GeometryHelpers::constructMat(t,r);
     //               std::cout << "    RecMat: " << m2 << std::endl;
     //               GeometryHelpers::decomposeMat(m2, r);
     //               std::cout << "    reRecRot: " << r[0] << "," << r[1] << "," << r[2] << "," << r[3] << std::endl;
     //           }
     //       }
            
            // Output best camera transformation
            Matrix4f globalCameraMatrix = Matrix4f();
            cv::Mat bestGlobalTransform;
            markerTracker->getBestCameraTransformation(currentCamera, bestGlobalTransform ,startTime);
            if (!bestGlobalTransform.empty()) {
                globalCameraMatrix = Matrix4f((float*) bestGlobalTransform.data, true);
                globalCameraMatrix.mult( rotationMatrix );
            }
            
            // Output the image with markers...
            cv::Mat res = markerTracker->getImage(currentCamera, startTime).clone();
            ImagePtr MIptr;
            IplImage newMI;
            //if (useInternalInput) {
                cv::flip(res, res, 0);
            //}
            newMI = IplImage(res);
            MIptr->setParameters(newMI.width, newMI.height, Image::FMT_BGR24);
            memcpy(MIptr->getBuffer(), newMI.imageData, newMI.imageSize);
            
            if (!frameNumbers[currentCamera].empty()) {
                frameNumberMutex.lock();
                long frameNumber = frameNumbers[currentCamera].front();
                frameNumbers[currentCamera].pop();
                frameNumberMutex.unlock();
                //std::cout << " Frame number: " << frameNumber << std::endl;
                markerFrameNumber->push(frameNumber);
            } else {
                addLog("Warning: No frame number available for current image!");
            }
            
            std::map<std::string, cv::Mat> dynamicBodyTransforms;
            MFStringPtr dynBodyNames;
            MFMatrix4fPtr dynBodyMatrices;
            markerTracker->getDynamicBodyTransformations(this->currentCamera, dynamicBodyTransforms, startTime);
            dynBodyNames->clear();
            dynBodyMatrices->clear();
            for (std::map<std::string,cv::Mat>::iterator d = dynamicBodyTransforms.begin(); d!= dynamicBodyTransforms.end(); ++d) {
                dynBodyNames->push_back(d->first);
				cv::Mat m = d->second.inv();
                Matrix4f matrix = Matrix4f((float*) m.data, true);
                matrix.mult( rotationMatrix );
                dynBodyMatrices->push_back(matrix);
            }

            dynamicBodyNames->push(dynBodyNames);
            dynamicBodyMatrices->push(dynBodyMatrices);

            globalCameraTransform->push(globalCameraMatrix);
            outputMarkerImage->push(MIptr);

			//// Output image
            ImagePtr ptr;
			cv::Mat mat = markerTracker->getOriginalImage(this->currentCamera, startTime);
            IplImage newIpl;
            //if (useInternalInput) {
				cv::flip(mat, mat, 0);
			//}
            newIpl = IplImage(mat);
            ptr->setParameters(newIpl.width, newIpl.height, Image::FMT_BGR24);
            memcpy(ptr->getBuffer(), newIpl.imageData, newIpl.imageSize);
            outputImage->push(ptr);
        }
        

    }
    
    

} // namespace InstantIO
