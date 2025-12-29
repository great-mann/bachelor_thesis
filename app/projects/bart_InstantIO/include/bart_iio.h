#ifndef __INSTANTIO_IRIOMARKERTRACKER_H
#define __INSTANTIO_IRIOMARKERTRACKER_H


#include <string>
#include <queue>

#include <InstantIO/InstantIODef.h>
#include <InstantIO/ThreadedNode.h>
#include <InstantIO/NodeType.h>
#include <InstantIO/Image.h>
#include <InstantIO/SmartPtr.h>
#include <InstantIO/MFTypes.h>
#include <InstantIO/TypeName.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <boost/thread.hpp>

#include <bart/MarkerTracker.h>


namespace InstantIO
{

template <class T> class OutSlot;

/**
 * @class bart_iio bart_iio.h 
 *		                          InstantIO/bart_iio.h
 *
 * InstantIO-C++-template (www.instantreality.org)
 */
class INSTANTIO_DLLMAPPING bart_iio : public ThreadedNode
{
public:
    bart_iio();
    virtual ~bart_iio();

    // Create setter and getter methods for a field.
	INSTANTIO_SETTER_GETTER(std::string, ConfigUrl, configUrl);
	INSTANTIO_SETTER_GETTER(bool, FlipImage, flipImage);
	INSTANTIO_SETTER_GETTER(bool, UseInternalInput, useInternalInput);
	INSTANTIO_SETTER_GETTER(int, InternalInputCameraNumber, internalInputCameraNumber);
	INSTANTIO_SETTER_GETTER(long, InternalWidth, width);
	INSTANTIO_SETTER_GETTER(long, InternalHeight, height);
	INSTANTIO_SETTER_GETTER(bool, MarkerCoordinateSystemIsXZ, markerCoordinateSystemIsXZ); 
	INSTANTIO_SETTER_GETTER(std::string, InputFormat, inputFormat);

  
    // Factory method to create an instance of bart_iio.
    static Node *create();
  
    // Factory method to return the type of FooNode.
    virtual NodeType *type() const;
  
protected:
    // Gets called when the FooNode is enabled.
    virtual void initialize();
  
    // Gets called when the FooNode is disabled.
    virtual void shutdown();
  
    // thread method to send/receive data.
    virtual int processData();
  
private:
    bart_iio(const bart_iio &);
    const bart_iio &operator=(const bart_iio &);
    
    // Callback for detections
    void callback(MarkerTrackerState state, boost::posix_time::ptime startTime);
  
    // A "static" field of FooNode.
	std::string configUrl;
	long width;
	long height;
	int internalInputCameraNumber;
	bool markerCoordinateSystemIsXZ;
	bool useInternalInput;
    bool flipImage;
	std::string inputFormat;
  
    // Dynamic slots
	InSlot<std::string>    *cameraId;
	InSlot<ImagePtr>    *inputImage;
	InSlot<long>   *frameNumber;
    OutSlot<MFMatrix4fPtr>  *markerMatrizes;
    OutSlot<MFMatrix4fPtr>  *globalMarkerMatrizes;
    OutSlot<Matrix4f>  *globalCameraTransform;
	OutSlot<MFInt32Ptr> *markerIds;
	OutSlot<MFStringPtr> *dynamicBodyNames;
	OutSlot<MFMatrix4fPtr> *dynamicBodyMatrices;
	OutSlot<ImagePtr>   *outputImage;
	OutSlot<ImagePtr>   *outputMarkerImage;
    OutSlot<float>   *fieldOfView;
	OutSlot<long>   *markerFrameNumber;
    
    // QR-Code Generator
    InSlot<std::string> *qrString;
    OutSlot<ImagePtr>   *qrImage;
    
  
    // Type and type attributes
    static NodeType type_;
    static const char *typeName_;
    static const char *shortDescription_;
    static const char *longDescription_;
    static const char *author_;
    static Field fields_[8];
    
    boost::posix_time::ptime startTime;

	// Marker Tracker
	MarkerTracker *markerTracker;

    // found ids in a frame
    std::vector<int> foundIds;
    
    std::string currentCamera;
    
    // Frame number of the input
    std::map<std::string, std::queue<long> > frameNumbers;
    boost::mutex frameNumberMutex;

	// Video capture for internal input
	cv::VideoCapture capture;
    

};

} // namespace InstantIO


#endif // __INSTANTIO_IRIOMARKERTRACKER_H
