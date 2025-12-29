#include "DetectionLegacy.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>

int main() {
    Stopwatch sw;
    DetectionLegacy detector(sw);
        
    cv::Mat inputImage2 = cv::imread("photo2.jpg", cv::IMREAD_GRAYSCALE);
    cv::Mat binary2;
    detector.performThreshold(inputImage2, binary2);
    //show binary image until key pressed
    cv::imshow("Binary Image", binary2);
    cv::waitKey(0);
    cv::Mat inputImage = cv::imread("photo.jpg", cv::IMREAD_GRAYSCALE);
    cv::Mat binary;
    detector.performThreshold(inputImage, binary);
    //show binary image until key pressed
    cv::imshow("Binary Image", binary);
    cv::waitKey(0);
    //save pictures to disk
    cv::imwrite("keyboard.jpg", binary2);
    cv::imwrite("environment.jpg", binary);
    return 0;
}
