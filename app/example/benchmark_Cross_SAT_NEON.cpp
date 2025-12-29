#include "DetectionLegacy.h"
#include <filesystem>
#include <boost/thread.hpp>
#include <boost/program_options.hpp>
#include <opencv2/aruco.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string>
#include <queue>
#include <limits>
int main() {
    DetectionLegacy detector_(*(new Stopwatch()));
    cv::Mat gray = cv::imread("../data_from_host/photo.jpg", cv::IMREAD_GRAYSCALE);
    if (gray.empty()) {
        std::cerr << "Failed to load image\n";
        return 1;
    }
    cv::Mat binary;

    const int iters = 50;
    for(int i = 0; i < iters; i++) {
        detector_.DetectionLegacy::performThresholdAdaptiveCross_Neon(gray, binary);
    }
    return 0;

}