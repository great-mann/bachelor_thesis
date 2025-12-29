#include "DetectionLegacy.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <string>
#include <functional>
#include <filesystem>
#include <vector>
#include <chrono>

struct Row {
    int frame;
    int proc_pixels;
    double ms;
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <variant> --file=/path/to/video\n";
        return 1;
    }

    std::string variant;
    std::string video_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--file=", 0) == 0) video_path = arg.substr(7);
        else if (variant.empty()) variant = arg;
    }

    if (variant.empty()) { std::cerr << "Missing variant\n"; return 1; }
    if (video_path.empty()) { std::cerr << "Missing --file= argument\n"; return 1; }

    Stopwatch sw;
    DetectionLegacy detector(sw);

    using DetectorCall = std::function<void(const cv::Mat&, cv::Mat&)>;

    std::unordered_map<std::string, DetectorCall> variants = {
        { "v1", [&](const cv::Mat& g, cv::Mat& b){ detector.performThresholdAdaptiveSAT_NEON16(g, b); } },
        { "v2", [&](const cv::Mat& g, cv::Mat& b){ detector.performThreshold(g, b); } },
        { "v3", [&](const cv::Mat& g, cv::Mat& b){ detector.performThresholdAdaptiveSAT_NEON8(g, b); } },
        { "v4", [&](const cv::Mat& g, cv::Mat& b){ detector.adaptiveThresholdMeanFast(g, b, 255.0, 15, 0.9); } },
        { "v5", [&](const cv::Mat& g, cv::Mat& b){ detector.performThresholdAdaptiveSAT(g, b); } },
        { "v6", [&](const cv::Mat& g, cv::Mat& b){ detector.performThresholdAdaptiveSAT_NEON4(g, b); } }
    };

    auto it = variants.find(variant);
    if (it == variants.end()) {
        std::cerr << "Unknown variant: " << variant << "\n";
        return 1;
    }
    DetectorCall selected_fn = it->second;

    cv::VideoCapture cap(video_path);
    if (!cap.isOpened()) {
        std::cerr << "Failed to open video: " << video_path << "\n";
        return 1;
    }

    cv::Mat frame, gray, binary;

    // Warm-up runs
    for (int i = 0; i < 30; ++i) {
        if (!cap.read(frame)) break;
        if (frame.channels() == 3) cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        else gray = frame;
        selected_fn(gray, binary);
    }
    cap.set(cv::CAP_PROP_POS_FRAMES, 0);

    // Prepare output
    std::filesystem::create_directories("final_results_thresholding");
    const std::string out_csv = "final_results_thresholding/threshold_times.csv";
    const bool exists = std::filesystem::exists(out_csv);

    std::ofstream log(out_csv, std::ios::app);
    if (!log.is_open()) {
        std::cerr << "Failed to open log file: " << out_csv << "\n";
        return 1;
    }
    if (!exists) log << "which,proc_pixels,ms_threshold,frame\n";

    // per frame timings
    std::vector<Row> rows;
    const int total_frames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    if (total_frames > 0) rows.reserve(total_frames); 
    else rows.reserve(100000);                         

    int frame_idx = 0;
    while (cap.read(frame)) {
        if (frame.channels() == 3) cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        else gray = frame;

        auto t0 = std::chrono::high_resolution_clock::now();
        selected_fn(gray, binary);
        auto t1 = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        int proc_pixels = gray.cols * gray.rows;

        rows.push_back({frame_idx, proc_pixels, ms});
        frame_idx++;
    }

    // Write once at the end becuse of the hot loop 
    for (const auto& r : rows) {
        log << variant << "," << r.proc_pixels << "," << r.ms << "," << r.frame << "\n";
    }

    std::cout << "Done. Frames: " << frame_idx << "\n";
    return 0;
}
