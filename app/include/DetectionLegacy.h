#pragma once

#include <vector>
#include <optional>
#include <opencv2/core.hpp>
#include "Stopwatch.h"
#include "Marker.h"
#include "MarkerTrackerConfig.h"
#include "Anytime.h"
#include "ThresholdMethod.h"
struct SatCacheEntry {
    cv::Size size;  // the image size that corresponds to this SAT
    cv::Mat sat;    // Integral image 
};
class DetectionLegacy : public Anytime {

public:

    explicit DetectionLegacy(Stopwatch& sw, cv::Size markerSizePixels = {80, 80},  MarkerTrackerConfig* config = nullptr)
        : markerSizePixels_(markerSizePixels), m_stopwatch(sw), config(config) {
        markerCorners2d_[0] = cv::Point2f(0, 0);
        markerCorners2d_[1] = cv::Point2f(markerSizePixels_.width - 1, 0);
        markerCorners2d_[2] = cv::Point2f(markerSizePixels_.width - 1, markerSizePixels_.height - 1);
        markerCorners2d_[3] = cv::Point2f(0, markerSizePixels_.height - 1);
    }
    void prepareImage(const cv::Mat& bgrOrGray, cv::Mat& grayscale) const;
    void performThreshold(const cv::Mat& grayscale, cv::Mat& thresholdImg) const;
    void performThresholdOTSU(const cv::Mat& grayscale, cv::Mat& thresholdImg) const;
    void performThresholdAdaptiveSAT(const cv::Mat& grayscale, cv::Mat& thresholdImg) const;
    void performThresholdAdaptiveSAT_NEON16(const cv::Mat& gray,
                                                       cv::Mat& binary) const;
    void performThresholdAdaptiveSAT_NEON8(const cv::Mat& gray,
                                                       cv::Mat& binary) const;
    void adaptiveThresholdMeanFast(const cv::Mat& src,
                               cv::Mat& dst,
                               double maxValue,
                               int blockSize,
                               double C) const;
    void performThresholdAdaptiveSAT_NEON4(const cv::Mat& gray, cv::Mat& binary) const;


    void findContours(cv::Mat& thresholdImg,
                      std::vector<std::vector<cv::Point>>& contours,
                      int minContourPointsAllowed) const;

    void refineCorners(const cv::Mat& grayscale, Marker& marker) const;
    bool recognizeMarker(const cv::Mat& grayscale, Marker& marker) const;

    bool findMarkers(const cv::Mat& scaled,
                     const cv::Mat& grayscale,
                     std::vector<Marker>& detectedMarkers,
                     cv::Point2f deviation,
                     bool skip3d,
                     ThresholdMethod method,
                     double scalingFactor  = 1.0);

    bool findMarkersWithPrior(const cv::Mat& scaled,
                              const cv::Mat& grayscale,
                              float roiScale,
                              Marker& priorMarker,
                              std::vector<Marker>& detectedMarkers,
                              ThresholdMethod method);

    bool estimateMarker(const cv::Mat& scaled,
                        const cv::Mat& grayscale,
                        float roiScale,
                        Marker& priorMarker) const;

    void findCandidates(const std::vector<std::vector<cv::Point> > &contours,
                            std::vector<Marker> &detectedMarkers) const;

private:
    cv::Size   markerSizePixels_;
    cv::Point2f markerCorners2d_[4];
    Stopwatch& m_stopwatch;
    MarkerTrackerConfig* config;
    //std::unordered_map<double, SatCacheEntry> m_satCache;
    boost::mutex m_satCacheMutex;


};

