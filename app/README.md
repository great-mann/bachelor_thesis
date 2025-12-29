init

How to build.

set BUILD_BART_JAVA ON or OFF if you want it to build or not

example for cmaking from command line without bart_java:

bart/build> cmake -DBUILD_BART_JAVA=OFF ..

Requirements: 

All platforms:
- Boost (> 1.57)
- CMake
- OpenCV (> 2.4.9)

Windows (tested versions):
- Visual Studio 2013
- Visual Studio 2008



Installation:

Windows:
- Create a build directory for the compilation process, e.g. build-bart-vc12
- Configure the cmake script
- - Generate for Visual Studio 12 2013 Win64.


ker --full 
aaab threshOpt: adaptive
Threshold method: adaptive
Setup...
Setup done.
Opening internal camera...
Using pipeline: libcamerasrc ! video/x-raw,format=NV21,width=640,height=480,framerate=30/1 ! videoconvert ! video/x-raw,format=NV21 ! appsink drop=true max-buffers=1 sync=false
[0:01:47.038973891] [1908]  INFO Camera camera_manager.cpp:330 libcamera v0.5.2+99-bfd68f78
[0:01:47.077804071] [1922]  INFO IPAProxy ipa_proxy.cpp:180 Using tuning file /usr/share/libcamera/ipa/rpi/vc4/imx296.json
[0:01:47.082199906] [1922]  INFO Camera camera_manager.cpp:220 Adding camera '/base/soc/i2c0mux/i2c@1/imx296@1a' for pipeline handler rpi/vc4
[0:01:47.082297440] [1922]  INFO RPI vc4.cpp:440 Registered camera /base/soc/i2c0mux/i2c@1/imx296@1a to Unicam device /dev/media2 and ISP device /dev/media1
[0:01:47.085320504] [1925]  INFO Camera camera.cpp:1215 configuring streams: (0) 640x480-NV21/Rec709
[0:01:47.085764802] [1922]  INFO RPI vc4.cpp:615 Sensor: /base/soc/i2c0mux/i2c@1/imx296@1a - Selected sensor format: 1456x1088-SBGGR10_1X10/RAW - Selected unicam format: 1456x1088-pBAA/RAW
[ WARN:0@1.071] global cap_gstreamer.cpp:1754 open OpenCV | GStreamer warning: unable to query duration of stream
[ WARN:0@1.071] global cap_gstreamer.cpp:1777 open OpenCV | GStreamer warning: Cannot quer

