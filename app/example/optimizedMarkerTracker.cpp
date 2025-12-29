#include "testMarkerTracker.h"
#include <filesystem>
#include <boost/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/lockfree/queue.hpp>
#include <opencv2/aruco.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string>
#include <queue>
#include <limits>
#include <fstream>
#include <iomanip>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <vector>
#include <filesystem>
#include <random>

static std::atomic<uint64_t> g_read_frames{0};
static std::atomic<uint64_t> g_done_frames{0};
static std::atomic<uint64_t> g_timeout_frames{0};
static std::atomic<double>   g_last_latency_ms{0.0};
static std::atomic<uint64_t> g_cb_frame_id{0};
static std::mutex g_det_mtx;
static std::atomic<uint64_t> g_det_dropped{0};
static std::atomic<uint64_t> g_perf_dropped{0};
namespace po = boost::program_options;
namespace fs = std::filesystem;
using namespace std;

// ============================================================================
// Lock-free detection record structure
// ============================================================================

struct DetectionRecord {
    uint64_t frame_id;
    int64_t ts_us;
    int state_int;
    int n_markers;
    int marker_id;
    double tx, ty, tz;
    double rx, ry, rz;
    
    DetectionRecord() : frame_id(0), ts_us(0), state_int(0), n_markers(0), 
                        marker_id(-1), tx(0), ty(0), tz(0), rx(0), ry(0), rz(0) {}
};

struct PerformanceRecord {
    uint64_t frame_id;
    int64_t ts_us;
    double ms_process;
    
    PerformanceRecord() : frame_id(0), ts_us(0), ms_process(0.0) {}
};

// Lock-free queues for async writing
static boost::lockfree::queue<DetectionRecord*, boost::lockfree::capacity<4096>> g_det_queue;
static boost::lockfree::queue<PerformanceRecord*, boost::lockfree::capacity<4096>> g_perf_queue;

static std::atomic<bool> g_writer_running{false};
static std::unique_ptr<std::thread> g_writer_thread;

struct FrameMeta {
    uint64_t frame_id;
    double ms_process;
};

static const boost::posix_time::ptime g_epoch(
    boost::gregorian::date(1970,1,1)
);

static inline int64_t ts_since_epoch_us(const boost::posix_time::ptime& t) {
    return (t - g_epoch).total_microseconds();
}

static std::mutex g_meta_mtx;
static std::unordered_map<int64_t, FrameMeta> g_meta_by_ts;

static bool g_write_detections = false;
static std::unique_ptr<std::ofstream> g_det_out;
static std::unique_ptr<std::ofstream> g_perf_out;

static std::atomic<uint64_t> g_frame_idx{0}; 
boost::posix_time::ptime startTime;
boost::mutex my_mutex;
std::unique_ptr<MarkerTracker> markerTracker;
bool do_logging = false;

std::deque<boost::posix_time::ptime> fullFrames;

cv::Mat outMat;
size_t numberFrames = 0;
size_t numberDetected = 0;
static ThresholdMethod thresholdMethod = ThresholdMethod::v1;

// ============================================================================
// Fast formatting helpers
// ============================================================================
static inline void format_detection_line(char* buf, size_t bufsize, const DetectionRecord& rec) {
    snprintf(buf, bufsize, "%llu,%lld,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
             (unsigned long long)rec.frame_id,
             (long long)rec.ts_us,
             rec.state_int,
             rec.n_markers,
             rec.marker_id,
             rec.tx, rec.ty, rec.tz,
             rec.rx, rec.ry, rec.rz);
}

static inline void format_performance_line(char* buf, size_t bufsize, const PerformanceRecord& rec) {
    snprintf(buf, bufsize, "%llu,%lld,%.3f\n",
             (unsigned long long)rec.frame_id,
             (long long)rec.ts_us,
             rec.ms_process);
}
std::string random_string(std::size_t length = 8) {
    static const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    static std::mt19937 rng{std::random_device{}()};
    static std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);

    std::string s;
    s.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
        s += charset[dist(rng)];
    }
    return s;
}

// ============================================================================
// Async writer thread
// ============================================================================
void writer_thread_func() {
    const size_t BATCH_SIZE = 64;
    const size_t LINE_BUF_SIZE = 256;
    
    std::vector<DetectionRecord*> det_batch;
    std::vector<PerformanceRecord*> perf_batch;
    det_batch.reserve(BATCH_SIZE);
    perf_batch.reserve(BATCH_SIZE);
    
    char line_buf[LINE_BUF_SIZE];
    std::string det_buffer;
    std::string perf_buffer;
    det_buffer.reserve(BATCH_SIZE * LINE_BUF_SIZE);
    perf_buffer.reserve(BATCH_SIZE * LINE_BUF_SIZE);
    
    while (g_writer_running.load(std::memory_order_acquire)) {
        det_batch.clear();
        perf_batch.clear();
        det_buffer.clear();
        perf_buffer.clear();
        
        // Drain detection queue
        DetectionRecord* drec = nullptr;
        while (det_batch.size() < BATCH_SIZE && g_det_queue.pop(drec)) {
            det_batch.push_back(drec);
        }
        
        // Drain performance queue
        PerformanceRecord* prec = nullptr;
        while (perf_batch.size() < BATCH_SIZE && g_perf_queue.pop(prec)) {
            perf_batch.push_back(prec);
        }
        
        // Format all detection records into buffer
        for (auto* rec : det_batch) {
            format_detection_line(line_buf, LINE_BUF_SIZE, *rec);
            det_buffer.append(line_buf);
            delete rec;
        }
        
        // Format all performance records into buffer
        for (auto* rec : perf_batch) {
            format_performance_line(line_buf, LINE_BUF_SIZE, *rec);
            perf_buffer.append(line_buf);
            delete rec;
        }
        
        // Write buffers in one shot
        if (!det_buffer.empty() && g_det_out && g_det_out->is_open()) {
            g_det_out->write(det_buffer.data(), det_buffer.size());
        }
        
        if (!perf_buffer.empty() && g_perf_out && g_perf_out->is_open()) {
            g_perf_out->write(perf_buffer.data(), perf_buffer.size());
        }
        
        // Periodic flush (not every batch)
        static int flush_counter = 0;
        if (++flush_counter % 10 == 0) {
            if (g_det_out) g_det_out->flush();
            if (g_perf_out) g_perf_out->flush();
        }
        
        // Sleep if queues empty
        if (det_batch.empty() && perf_batch.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    
    // Final drain on shutdown
    DetectionRecord* drec = nullptr;
    while (g_det_queue.pop(drec)) {
        format_detection_line(line_buf, LINE_BUF_SIZE, *drec);
        if (g_det_out) g_det_out->write(line_buf, strlen(line_buf));
        delete drec;
    }
    
    PerformanceRecord* prec = nullptr;
    while (g_perf_queue.pop(prec)) {
        format_performance_line(line_buf, LINE_BUF_SIZE, *prec);
        if (g_perf_out) g_perf_out->write(line_buf, strlen(line_buf));
        delete prec;
    }
    
    if (g_det_out) g_det_out->flush();
    if (g_perf_out) g_perf_out->flush();
}

// ============================================================================
// Utility functions
// ============================================================================
static inline void matToRt(const cv::Mat& T, cv::Mat& rvec, cv::Mat& tvec) {
    cv::Mat R = T(cv::Rect(0,0,3,3)).clone();
    tvec = T(cv::Rect(3,0,1,3)).clone();
    cv::Rodrigues(R, rvec);
}

static inline void ensure64(cv::Mat& M) {
    if (M.depth() != CV_64F) M.convertTo(M, CV_64F);
}

void drawAxisFromT(cv::Mat& frame,               
                   const cv::Mat& T_in,
                   const cv::Mat& K_full_64,
                   const cv::Mat& D_full_64,
                   const cv::Rect& roi,
                   double axisLen = 0.05)
{
    CV_Assert(T_in.cols >= 4 && T_in.rows >= 3);
    cv::Mat T; T_in.convertTo(T, CV_64F);

    cv::Mat R = T(cv::Rect(0, 0, 3, 3)).clone();
    cv::Mat t = T(cv::Rect(3, 0, 1, 3)).clone();

    if (t.at<double>(2) <= 0 || !cv::checkRange(R) || !cv::checkRange(t)) return;

    cv::Mat rvec;
    cv::Rodrigues(R, rvec);

    cv::Mat K = K_full_64.clone();
    if (roi.x != 0 || roi.y != 0) {
        K.at<double>(0,2) -= roi.x;
        K.at<double>(1,2) -= roi.y;
    }

    cv::InputArray D = D_full_64.empty() ? cv::noArray() : cv::InputArray(D_full_64);
}

// ============================================================================
// Optimized callback - minimal work, just enqueue
// ============================================================================
void callback(MarkerTrackerState state, boost::posix_time::ptime frameTime)
{
    if (state != DONE && state != TIMEOUT) return;

    const std::string cam = "default";
    const int64_t ts_us = ts_since_epoch_us(frameTime);
    const uint64_t frame_id = g_cb_frame_id.fetch_add(1, std::memory_order_relaxed);
    const int state_int = (state == DONE) ? 1 : 0;

    // Update output image (optional, mutex protected)
    {
        cv::Mat res = markerTracker->getImage(cam, frameTime);
        boost::mutex::scoped_lock lk(my_mutex);
        if (!res.empty()) cv::resize(res, outMat, cv::Size(640, 480), 0, 0, cv::INTER_LINEAR);
        else outMat.release();
    }

    if (!g_write_detections) return;

    // Get detections once
    std::map<int, cv::Mat> mt;
    markerTracker->getMarkerTransformations(mt);
    const int n = (int)mt.size();

        if (n == 0) {
            auto* rec = new DetectionRecord();
            rec->frame_id   = frame_id;
            rec->ts_us      = ts_us;
            rec->state_int  = state_int;
            rec->n_markers  = 0;
            rec->marker_id  = -1;

            if (!g_det_queue.push(rec)) {
                delete rec;
                g_det_dropped.fetch_add(1, std::memory_order_relaxed);
            }
            return;
        }
 else {
        // Enqueue one record per marker
        for (const auto& kv : mt) {
            int id = kv.first;
            cv::Mat T = kv.second;
            T.convertTo(T, CV_64F);
            if (T.rows < 3 || T.cols < 4) continue;

            cv::Mat rvec, tvec;
            matToRt(T, rvec, tvec);
            ensure64(rvec);
            ensure64(tvec);

            DetectionRecord* rec = new DetectionRecord();
            rec->frame_id = frame_id;
            rec->ts_us = ts_us;
            rec->state_int = state_int;
            rec->n_markers = n;
            rec->marker_id = id;
            rec->tx = tvec.at<double>(0);
            rec->ty = tvec.at<double>(1);
            rec->tz = tvec.at<double>(2);
            rec->rx = rvec.at<double>(0);
            rec->ry = rvec.at<double>(1);
            rec->rz = rvec.at<double>(2);
            
            if (!g_det_queue.push(rec)) {
                delete rec;
                g_det_dropped.fetch_add(1, std::memory_order_relaxed);
            }

        }
    }
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char* argv[]) {
    cv::setUseOptimized(false);
    cv::setNumThreads(1);
    cv::setUseOptimized(false);

    std::cout << "Starting Main Application ";
    string configURL="";
    string captureFilename="";
    bool captureFile = false;
    bool show_input = false;
    bool show_markers = true;
    int camera_device = 0;
    int fps = -1;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("config", po::value<string>(), "configuration file describing the camera and marker configuration (.yaml) (DISABLED for now)")
        ("intrinsics", po::value<std::string>()->default_value("camera_intrinsics.yml"),
            "OpenCV YAML with camera_matrix and distortion_coefficients")
        ("device", po::value<int>(), "number of the camera device to be used")
        ("fps", po::value<int>(), "desired frame rate for playback or camera capture")
        ("file", po::value<string>(), "filename of a video for offline analysis")
        ("show_input", po::value<bool>(), "set to true if the input image should be shown")
        ("show_markers", po::value<bool>(), "set to false if the image with highlighted markers should not be shown")
        ("logging", po::value<bool>(), "set to true to enable logging statistic")
        ("threshold", po::value<std::string>()->default_value("v1"),
            "Thresholding method: otsu | v1 | v2 | v3 | v4 | v5");
    
    bool opt_full = false;
    bool opt_time_limited = false;
    bool opt_background = false;
    bool opt_skip_prior = false;
    int opt_timeout = 300;
    double opt_downscale = 0.5;
    int opt_scales = 0;

    desc.add_options()
        ("full", po::bool_switch(&opt_full), "run FULL mode (sync)")
        ("time_limited", po::bool_switch(&opt_time_limited), "run TIME_LIMITED mode")
        ("background", po::bool_switch(&opt_background), "run BACKGROUND mode")
        ("skip_prior", po::bool_switch(&opt_skip_prior), "disable prior-based ROI")
        ("timeout", po::value<int>(&opt_timeout)->default_value(300), "timeout ms")
        ("downscale", po::value<double>(&opt_downscale)->default_value(0.5), "downscale factor")
        ("scales", po::value<int>(&opt_scales)->default_value(0), "number of scales");

    std::string run_id = random_string();

    std::filesystem::path base_dir = std::filesystem::path("results") / run_id;
    //std::filesystem::create_directories(base_dir);

    std::filesystem::path det__path  = base_dir / "detections.csv";
    std::filesystem::path perf__path = base_dir / "performance_log.csv";


    std::string det_out_path  = det__path.string();
    std::string perf_out_path = perf__path.string();


desc.add_options()
    ("show_detections", po::bool_switch(&g_write_detections),
        "write per-frame detections AND performance logs")
    ("detections_out",
        po::value<std::string>(&det_out_path)->default_value(det_out_path),
        "detections CSV output")
    ("performance_out",
        po::value<std::string>(&perf_out_path)->default_value(perf_out_path),
        "performance CSV output");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    
    int options = 0;
    int mode_count = 0;

    if (opt_full) { options |= BartTrackingOptions::FULL; mode_count++; }
    if (opt_time_limited) { options |= BartTrackingOptions::TIME_LIMITED; mode_count++; }
    if (opt_background) { options |= BartTrackingOptions::BACKGROUND; mode_count++; }
    if (opt_skip_prior) { options |= BartTrackingOptions::SKIP_PRIORSEARCH; }

    if (mode_count != 1) {
        std::cerr << "Pick exactly one mode: --full OR --time_limited OR --background\n";
        return 1;
    }
    
    std::string threshOpt = vm["threshold"].as<std::string>();
    ThresholdMethod thresholdMethod;
    if (threshOpt == "otsu") thresholdMethod = ThresholdMethod::OTSU;
    else if (threshOpt == "v1") thresholdMethod = ThresholdMethod::v1;
    else if (threshOpt == "v2") thresholdMethod = ThresholdMethod::v2;
    else if (threshOpt == "v3") thresholdMethod = ThresholdMethod::v3;
    else if (threshOpt == "v4") thresholdMethod = ThresholdMethod::v4;
    else if (threshOpt == "v5") thresholdMethod = ThresholdMethod::v5;
    else if (threshOpt == "v6") thresholdMethod = ThresholdMethod::v6;

    else thresholdMethod = ThresholdMethod::v1;

    if (vm.count("help")) { cout << desc << "\n"; return 1; }
    if (vm.count("file")) { captureFilename = vm["file"].as<string>(); captureFile = true; }
    if (vm.count("config")) { configURL = vm["config"].as<string>(); }
    if (vm.count("fps")) { fps = vm["fps"].as<int>(); }
    if (vm.count("show_input")) { show_input = vm["show_input"].as<bool>(); }
    if (vm.count("show_markers")) { show_markers = vm["show_markers"].as<bool>(); }
    if (vm.count("logging")) { do_logging = vm["logging"].as<bool>(); }

    cv::Size2f markerSizeMeters(0.045f, 0.045f);
    markerTracker = std::make_unique<MarkerTracker>(markerSizeMeters, thresholdMethod);
    
    // Load intrinsics
    std::string intrPath = vm["intrinsics"].as<std::string>();
    cv::FileStorage ifs(intrPath, cv::FileStorage::READ);
    if (!ifs.isOpened()) {
        std::cerr << "Failed to open intrinsics file: " << intrPath << "\n";
        return 1;
    }

    cv::Mat K, D;
    ifs["camera_matrix"] >> K;
    ifs["distortion_coefficients"] >> D;
    if (K.empty()) ifs["cameraMatrix"] >> K;
    if (D.empty()) ifs["distCoeffs"] >> D;
    ifs.release();

    if (K.empty()) {
        std::cerr << "Intrinsics file missing camera_matrix\n";
        return 1;
    }

    markerTracker->setCameraIntrinsics("default", K, D);
    markerTracker->refreshCamera("default");
    markerTracker->registerCallback(boost::bind(&callback, boost::placeholders::_1, boost::placeholders::_2));

    if (show_markers) cv::namedWindow("Output", cv::WINDOW_AUTOSIZE);
    if (show_input) cv::namedWindow("Input", cv::WINDOW_AUTOSIZE);
    
    // Start async writer thread
    if (g_write_detections) {
        g_det_out = std::make_unique<std::ofstream>(det_out_path, std::ios::out | std::ios::trunc);
        g_perf_out = std::make_unique<std::ofstream>(perf_out_path, std::ios::out | std::ios::trunc);

        if (!g_det_out->is_open() || !g_perf_out->is_open()) {
            std::cerr << "Failed to open detection/performance logs\n";
            return 1;
        }

        (*g_det_out) << "frame,ts_us,state,n_markers,id,tx,ty,tz,rx,ry,rz\n";
        (*g_perf_out) << "frame,ts_us,ms_process\n";
        g_det_out->flush();
        g_perf_out->flush();
        
        g_writer_running.store(true, std::memory_order_release);
        g_writer_thread = std::make_unique<std::thread>(writer_thread_func);
    }

    cv::Mat mat;
    char c = 0;
    cv::VideoCapture capture;

    if (captureFile) {
        capture.open(captureFilename);
        if (!capture.isOpened()) {
            std::cerr << "Failed to open video file: " << captureFilename << "\n";
            return 1;
        }
    } else {
        int width = 640, height = 480;
        std::stringstream pipeline;
        //throws the frames out very quickly if drop=true
        //pipeline << "libcamerasrc ! "
          //      << "video/x-raw,format=NV21,width=" << width
            //    << ",height=" << height
              //  << ",framerate=30/1 ! "
                //<< "videoconvert ! video/x-raw,format=NV21 ! "
                //<< "appsink drop=true max-buffers=1 sync=false";
        pipeline << "libcamerasrc ! "
        << "video/x-raw,format=NV21,width=" << width
        << ",height=" << height
        << ",framerate=30/1 ! "
        << "videoconvert ! video/x-raw,format=NV21 ! "
        << "appsink drop=false max-buffers=30 sync=false";

        capture.open(pipeline.str(), cv::CAP_GSTREAMER);
        if (!capture.isOpened()) {
            std::cerr << "Failed to open camera pipeline.\n";
            return 1;
        }
    }

    startTime = boost::posix_time::microsec_clock::local_time();
    size_t frameNumber = 0;
    
    while (capture.read(mat) && c != 'x' && c != 'X') {
        if (mat.empty()) continue;

        // Handle NV21 conversion for camera
        if (!captureFile && mat.type() == CV_8UC1) {
            if (mat.rows == 720 && mat.cols == 640) {
                cv::Mat bgr;
                cv::cvtColor(mat, bgr, cv::COLOR_YUV2BGR_NV21);
                mat = bgr;
            }
        } else if (captureFile && mat.channels() == 1) {
            cv::cvtColor(mat, mat, cv::COLOR_GRAY2BGR);
        }

        boost::posix_time::ptime frameStartTime = boost::posix_time::microsec_clock::local_time();
        
        if (do_logging) cout << "Frame " << frameNumber << "\n";
        if (show_input) {
            cv::imshow("Input", mat);
            c = cv::waitKey(5);
        }

        try {
            if (!mat.empty() && mat.data != nullptr) {
                cv::Mat frameClone = mat.clone();
                boost::posix_time::ptime frameTime = boost::posix_time::microsec_clock::local_time();
                const int64_t ts_us = ts_since_epoch_us(frameTime);
                const uint64_t frame_id = g_frame_idx.fetch_add(1, std::memory_order_relaxed);

                {
                    std::lock_guard<std::mutex> lk(g_meta_mtx);
                    g_meta_by_ts[ts_us] = FrameMeta{frame_id, -1.0};
                }

                auto t0 = boost::posix_time::microsec_clock::local_time();
                markerTracker->processFrame(frameClone, frameTime, "default", options);
                //std::cout << "Processed frame " << frameNumber << "\n";
                //if(frameNumber ==0){
                    //create frames folder if it does not exist
                //    fs::create_directory("frames");
               // }
                //frameNumber++;
                //if(50 < frameNumber && frameNumber < 100){
                    //save frames for analysis in frames folder
                ///    std::stringstream ss;
                ///    ss << "frames/frame_" << std::setfill('0') << std::setw(4) << frameNumber << ".png";
               // /////    cv::imwrite(ss.str(), frameClone);  
               // }
                
                auto t1 = boost::posix_time::microsec_clock::local_time();
                double ms = (t1 - t0).total_microseconds() / 1000.0;
                
                // Enqueue performance record
                if (g_write_detections) {
                    auto* prec = new PerformanceRecord();
                    prec->frame_id   = frame_id;
                    prec->ts_us      = ts_us;
                    prec->ms_process = ms;

                    if (!g_perf_queue.push(prec)) {
                        delete prec;
                        g_perf_dropped.fetch_add(1, std::memory_order_relaxed);
                    }
                }


                {
                    std::lock_guard<std::mutex> lk(g_meta_mtx);
                    auto it = g_meta_by_ts.find(ts_us);
                    if (it != g_meta_by_ts.end()) it->second.ms_process = ms;
                }

                // FPS overlay
                boost::posix_time::ptime frameEnd = boost::posix_time::microsec_clock::local_time();
                double frameDurationMs = (frameEnd - frameStartTime).total_milliseconds();
                double currentFps = (frameDurationMs > 0) ? 1000.0 / frameDurationMs : 0.0;
                static double fps_smooth = 0.0;
                fps_smooth = (fps_smooth == 0.0) ? currentFps : 0.9 * fps_smooth + 0.1 * currentFps;
                double lat = g_last_latency_ms.load(std::memory_order_relaxed);

                if (show_markers && !outMat.empty()) {
                    cv::putText(outMat, cv::format("FPS: %.1f", fps_smooth),
                                cv::Point(20, 40), cv::FONT_HERSHEY_SIMPLEX,
                                1.0, cv::Scalar(0, 255, 0), 2);
                    cv::putText(outMat, cv::format("Latency: %.1f ms", lat),
                                cv::Point(20, 80), cv::FONT_HERSHEY_SIMPLEX,
                                0.9, cv::Scalar(0, 255, 255), 2);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing frame: " << e.what() << std::endl;
            continue;
        }
        
        if (show_markers) {
            boost::mutex::scoped_lock lock(my_mutex);
            if (!outMat.empty()) {
                cv::imshow("Output", outMat);
                c = cv::waitKey(1);
            }
        }
        
        if (fps > 0) {
            long diff = boost::posix_time::time_duration(
                boost::posix_time::microsec_clock::local_time() - frameStartTime
            ).total_milliseconds();
            long sleep_time = (1000/fps) - diff;
            if (sleep_time > 0) {
                boost::this_thread::sleep(boost::posix_time::milliseconds(sleep_time));
            }
        }
    }

    // Shutdown writer thread
    if (g_write_detections && g_writer_thread) {
        g_writer_running.store(false, std::memory_order_release);
        g_writer_thread->join();
    }

    if (captureFile) {
        std::cout << "Video ended...\n";
        return 0;
    }

    if (show_markers || show_input) {
        std::cout << "Press any key to exit...\n";
        cv::waitKey(0);
    }

    cv::destroyAllWindows();
    return 0;
}