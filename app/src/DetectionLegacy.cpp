#include "../include/DetectionLegacy.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <boost/thread.hpp>
#include <algorithm>
#include <fstream>
#include <arm_neon.h>
#include <opencv2/opencv.hpp>


static std::shared_ptr<Camera> g_camera;
static std::ofstream perfLog("perf_log.csv");
static std::ofstream perfLog_contours("perf_log_contours.csv");
static size_t frame_idx = 0;


//processed frame number 
//static uint64_t processedFrameNumber = 0;

void DetectionLegacy::prepareImage(const cv::Mat& bgraMat, cv::Mat& gray) const {
    if (bgraMat.channels() == 1) {
        gray = bgraMat;
    } else if (bgraMat.channels() == 3) {
        cv::cvtColor(bgraMat, gray, cv::COLOR_BGR2GRAY);
    } else if (bgraMat.channels() == 4) {
        cv::cvtColor(bgraMat, gray, cv::COLOR_BGRA2GRAY);
    } else {
        // fallback: 
        gray = bgraMat.clone();
    }
}
__attribute__((optimize("no-tree-vectorize")))
void DetectionLegacy::adaptiveThresholdMeanFast(const cv::Mat& src,
                               cv::Mat& dst,
                               double maxValue,
                               int blockSize,
                               double C) const
{
    CV_Assert(src.type() == CV_8UC1);
    CV_Assert(blockSize % 2 == 1 && blockSize >= 3);

    const int r = blockSize / 2;
    const int H = src.rows;
    const int W = src.cols;

    cv::Mat padded;
    cv::copyMakeBorder(src, padded, r, r, r, r, cv::BORDER_REPLICATE);

    const int Hp = padded.rows;

    // Horizontal sliding sum: Hp x W, 32-bit is fine for typical blockSize
    cv::Mat hsum(Hp, W, CV_32S);

    for (int y = 0; y < Hp; ++y) {
        const uchar* s = padded.ptr<uchar>(y);
        int* h = hsum.ptr<int>(y);

        int windowSum = 0;
        for (int k = 0; k < blockSize; ++k) windowSum += s[k];
        h[0] = windowSum;

        for (int x = 1; x < W; ++x) {
            windowSum += s[x + blockSize - 1] - s[x - 1];
            h[x] = windowSum;
        }
    }

    dst.create(H, W, CV_8UC1);

    const int area = blockSize * blockSize;
    const int Cint = (int)std::lround(C);
    const uchar maxV = (uchar)maxValue;

    // vSum[x] holds the vertical window sum for column x (over hsum)
    std::vector<int> vSum(W, 0);

    // Initialize vSum with rows [0 .. blockSize-1] of hsum
    for (int k = 0; k < blockSize; ++k) {
        const int* hk = hsum.ptr<int>(k);
        for (int x = 0; x < W; ++x) vSum[x] += hk[x];
    }

    // Process each output row y
    for (int y = 0; y < H; ++y) {
        const uchar* sRow = src.ptr<uchar>(y);
        uchar* dRow = dst.ptr<uchar>(y);

        for (int x = 0; x < W; ++x) {
            int mean = vSum[x] / area;          
            int thresh = mean - Cint;
            if (thresh < 0) thresh = 0;

            dRow[x] = (sRow[x] > thresh) ? maxV : 0;
        }

        // add row (y + blockSize) and subtract row (y)
        if (y + 1 < H) {
            const int* addRow = hsum.ptr<int>(y + blockSize);
            const int* subRow = hsum.ptr<int>(y);
            for (int x = 0; x < W; ++x) {
                vSum[x] += addRow[x] - subRow[x];
            }
        }
    }
}

void DetectionLegacy::performThreshold(const cv::Mat& gray, cv::Mat& binary) const {
    
    cv::adaptiveThreshold(gray, binary, 255,
                          cv::ADAPTIVE_THRESH_MEAN_C,
                          cv::THRESH_BINARY_INV,
                          15, 5);
     
                    //cv::imshow("Binary", binary);

                                 
}



void DetectionLegacy::performThresholdAdaptiveSAT_NEON8(const cv::Mat& gray, cv::Mat& binary) const
{
    CV_Assert(gray.type() == CV_8UC1);

    constexpr int R = 7;
    constexpr int WIN = 2 * R + 1;
    constexpr int area = WIN * WIN; // 225
    constexpr uint32_t FACTOR_Q16 = (uint32_t)std::lround(0.9 * 65536.0); // ~58982
    constexpr uint32_t LHS_SCALE  = (uint32_t)(area * 65536);            // 14,745,600

    const int rows = gray.rows;
    const int cols = gray.cols;

    binary.create(gray.size(), CV_8UC1);

    cv::Mat padded;
    cv::copyMakeBorder(gray, padded, R, R, R, R, cv::BORDER_REPLICATE);

    cv::Mat ii;
    cv::integral(padded, ii, CV_32S);

    // Vector constants (unsigned)
    const uint32x4_t vLHS = vdupq_n_u32(LHS_SCALE);
    const uint32x4_t vF   = vdupq_n_u32(FACTOR_Q16);
    const uint8x8_t  vFF  = vdup_n_u8(255);

    for (int y = 0; y < rows; ++y) {
        const uint8_t* gRow = gray.ptr<uint8_t>(y);
        uint8_t* bRow = binary.ptr<uint8_t>(y);

        const int y1 = y;
        const int y2 = y + WIN;

        const int32_t* row1 = ii.ptr<int32_t>(y1);
        const int32_t* row2 = ii.ptr<int32_t>(y2);

        int x = 0;

        // 8 pixels per iteration
        for (; x <= cols - 8; x += 8) {
            const int x1 = x;
            const int x2 = x + WIN;

            // Load 8 grayscale pixels
            uint8x8_t pix_u8 = vld1_u8(gRow + x);

            // Widen u8 -> u16 -> u32 (two groups of 4 lanes)
            uint16x8_t pix_u16 = vmovl_u8(pix_u8);
            uint32x4_t pix0 = vmovl_u16(vget_low_u16(pix_u16));   // x+0..x+3
            uint32x4_t pix1 = vmovl_u16(vget_high_u16(pix_u16));  // x+4..x+7

            // load A,B,C,D for lanes 0..3 and 4..7
            int32x4_t A0 = vld1q_s32(row1 + x1 + 0);
            int32x4_t B0 = vld1q_s32(row1 + x2 + 0);
            int32x4_t C0 = vld1q_s32(row2 + x1 + 0);
            int32x4_t D0 = vld1q_s32(row2 + x2 + 0);
            int32x4_t A1 = vld1q_s32(row1 + x1 + 4);
            int32x4_t B1 = vld1q_s32(row1 + x2 + 4);
            int32x4_t C1 = vld1q_s32(row2 + x1 + 4);
            int32x4_t D1 = vld1q_s32(row2 + x2 + 4);

            // sum = D - B - C + A 
            int32x4_t sum0 = vaddq_s32(vsubq_s32(vsubq_s32(D0, B0), C0), A0);
            int32x4_t sum1 = vaddq_s32(vsubq_s32(vsubq_s32(D1, B1), C1), A1);
            // Treat sums as unsigned for scaling and comparison
            uint32x4_t sum0u = vreinterpretq_u32_s32(sum0);
            uint32x4_t sum1u = vreinterpretq_u32_s32(sum1);

            // rhs = sum * FACTOR_Q16 (u32)
            uint32x4_t rhs0u = vmulq_u32(sum0u, vF);
            uint32x4_t rhs1u = vmulq_u32(sum1u, vF);

            // lhs = pix * LHS_SCALE (u32)
            uint32x4_t lhs0u = vmulq_u32(pix0, vLHS);
            uint32x4_t lhs1u = vmulq_u32(pix1, vLHS);

            // mask = (lhs < rhs) <=> (rhs > lhs), unsigned compare
            uint32x4_t m0 = vcgtq_u32(rhs0u, lhs0u);
            uint32x4_t m1 = vcgtq_u32(rhs1u, lhs1u);

            // Pack masks to 8 bytes (0xFF for true, 0x00 for false)
            uint16x4_t n0 = vmovn_u32(m0);
            uint16x4_t n1 = vmovn_u32(m1);
            uint16x8_t n01 = vcombine_u16(n0, n1);
            uint8x8_t out = vmovn_u16(n01);

            // Ensure 0/255
            out = vand_u8(out, vFF);

            vst1_u8(bRow + x, out);
        }

        // Scalar tail (correct)
        for (; x < cols; ++x) {
            const int x1 = x;
            const int x2 = x + WIN;

            const int32_t A = row1[x1];
            const int32_t B = row1[x2];
            const int32_t C = row2[x1];
            const int32_t D = row2[x2];

            const uint32_t sum = (uint32_t)(D - B - C + A);

            const uint32_t lhs = (uint32_t)gRow[x] * LHS_SCALE;
            const uint32_t rhs = sum * FACTOR_Q16;

            bRow[x] = (lhs < rhs) ? 255 : 0;
        }
    }
}

void DetectionLegacy::performThresholdAdaptiveSAT_NEON16(const cv::Mat& gray, cv::Mat& binary) const
{
    CV_Assert(gray.type() == CV_8UC1);

    constexpr int R = 7;
    constexpr int WIN = 2 * R + 1;
    constexpr int area = WIN * WIN; // 225
    constexpr uint32_t FACTOR_Q16 = (uint32_t)std::lround(0.9 * 65536.0); // ~58982
    constexpr uint32_t LHS_SCALE  = (uint32_t)(area * 65536);            // 14,745,600

    const int rows = gray.rows;
    const int cols = gray.cols;

    binary.create(gray.size(), CV_8UC1);

    cv::Mat padded;
    cv::copyMakeBorder(gray, padded, R, R, R, R, cv::BORDER_REPLICATE);

    cv::Mat ii;
    cv::integral(padded, ii, CV_32S);

    // Vector constants
    const uint32x4_t vLHS = vdupq_n_u32(LHS_SCALE);
    const uint32x4_t vF   = vdupq_n_u32(FACTOR_Q16);
    const uint8x16_t vFF  = vdupq_n_u8(255);

    for (int y = 0; y < rows; ++y) {
        const uint8_t* gRow = gray.ptr<uint8_t>(y);
        uint8_t* bRow = binary.ptr<uint8_t>(y);

        const int y1 = y;
        const int y2 = y + WIN;

        const int32_t* row1 = ii.ptr<int32_t>(y1);
        const int32_t* row2 = ii.ptr<int32_t>(y2);

        int x = 0;

        // 16 pixels per iteration
        for (; x <= cols - 16; x += 16) {
            const int x1 = x;
            const int x2 = x + WIN;

            // Load grayscale 16 bytes
            uint8x16_t pix_u8 = vld1q_u8(gRow + x);

            // widen ... four groups of 4 lanes)
            uint16x8_t pix_u16_lo = vmovl_u8(vget_low_u8(pix_u8));
            uint16x8_t pix_u16_hi = vmovl_u8(vget_high_u8(pix_u8));

            uint32x4_t pix0 = vmovl_u16(vget_low_u16(pix_u16_lo));   // x+0..x+3
            uint32x4_t pix1 = vmovl_u16(vget_high_u16(pix_u16_lo));  // x+4..x+7
            uint32x4_t pix2 = vmovl_u16(vget_low_u16(pix_u16_hi));   // x+8..x+11
            uint32x4_t pix3 = vmovl_u16(vget_high_u16(pix_u16_hi));  // x+12..x+15

            // Load A,B,C,D as 32-bit vectors 
            // sum = D - B - C + A 
            int32x4_t A0 = vld1q_s32(row1 + x1 + 0);
            int32x4_t A1 = vld1q_s32(row1 + x1 + 4);
            int32x4_t A2 = vld1q_s32(row1 + x1 + 8);
            int32x4_t A3 = vld1q_s32(row1 + x1 + 12);

            int32x4_t B0 = vld1q_s32(row1 + x2 + 0);
            int32x4_t B1 = vld1q_s32(row1 + x2 + 4);
            int32x4_t B2 = vld1q_s32(row1 + x2 + 8);
            int32x4_t B3 = vld1q_s32(row1 + x2 + 12);

            int32x4_t C0 = vld1q_s32(row2 + x1 + 0);
            int32x4_t C1 = vld1q_s32(row2 + x1 + 4);
            int32x4_t C2 = vld1q_s32(row2 + x1 + 8);
            int32x4_t C3 = vld1q_s32(row2 + x1 + 12);

            int32x4_t D0 = vld1q_s32(row2 + x2 + 0);
            int32x4_t D1 = vld1q_s32(row2 + x2 + 4);
            int32x4_t D2 = vld1q_s32(row2 + x2 + 8);
            int32x4_t D3 = vld1q_s32(row2 + x2 + 12);

            // ---- sum = (D - B - C) + A ----
            int32x4_t sum0 = vaddq_s32(vsubq_s32(vsubq_s32(D0, B0), C0), A0);
            int32x4_t sum1 = vaddq_s32(vsubq_s32(vsubq_s32(D1, B1), C1), A1);
            int32x4_t sum2 = vaddq_s32(vsubq_s32(vsubq_s32(D2, B2), C2), A2);
            int32x4_t sum3 = vaddq_s32(vsubq_s32(vsubq_s32(D3, B3), C3), A3);

            // sum is non-negative; treat as uint32 for scaling
            uint32x4_t sum0u = vreinterpretq_u32_s32(sum0);
            uint32x4_t sum1u = vreinterpretq_u32_s32(sum1);
            uint32x4_t sum2u = vreinterpretq_u32_s32(sum2);
            uint32x4_t sum3u = vreinterpretq_u32_s32(sum3);

            // ---- rhs = sum * FACTOR_Q16 (u32) ----
            uint32x4_t rhs0u = vmulq_u32(sum0u, vF);
            uint32x4_t rhs1u = vmulq_u32(sum1u, vF);
            uint32x4_t rhs2u = vmulq_u32(sum2u, vF);
            uint32x4_t rhs3u = vmulq_u32(sum3u, vF);

            // ---- lhs = pix * LHS_SCALE (u32) ----
            uint32x4_t lhs0u = vmulq_u32(pix0, vLHS);
            uint32x4_t lhs1u = vmulq_u32(pix1, vLHS);
            uint32x4_t lhs2u = vmulq_u32(pix2, vLHS);
            uint32x4_t lhs3u = vmulq_u32(pix3, vLHS);

            // We want (lhs < rhs) <=> (rhs > lhs), unsigned compare
            uint32x4_t m0 = vcgtq_u32(rhs0u, lhs0u);
            uint32x4_t m1 = vcgtq_u32(rhs1u, lhs1u);
            uint32x4_t m2 = vcgtq_u32(rhs2u, lhs2u);
            uint32x4_t m3 = vcgtq_u32(rhs3u, lhs3u);

            // Pack masks down to bytes (0xFF for true, 0x00 for false)
            uint16x4_t n0 = vmovn_u32(m0);
            uint16x4_t n1 = vmovn_u32(m1);
            uint16x4_t n2 = vmovn_u32(m2);
            uint16x4_t n3 = vmovn_u32(m3);

            uint16x8_t n01 = vcombine_u16(n0, n1);
            uint16x8_t n23 = vcombine_u16(n2, n3);

            uint8x8_t out0 = vmovn_u16(n01);
            uint8x8_t out1 = vmovn_u16(n23);

            uint8x16_t out = vcombine_u8(out0, out1);

            // Ensure exact 0/255
            out = vandq_u8(out, vFF);

            vst1q_u8(bRow + x, out);
        }

        // scalar tail for the rest of pixels
        for (; x < cols; ++x) {
            const int x1 = x;
            const int x2 = x + WIN;

            const int32_t A = row1[x1];
            const int32_t B = row1[x2];
            const int32_t C = row2[x1];
            const int32_t D = row2[x2];

            const uint32_t sum = (uint32_t)(D - B - C + A);
            const uint32_t lhs = (uint32_t)gRow[x] * LHS_SCALE;
            const uint32_t rhs = sum * FACTOR_Q16;

            bRow[x] = (lhs < rhs) ? 255 : 0;
        }
    }
                       cv::imshow("Binary", binary);

}
__attribute__((optimize("no-tree-vectorize")))
void DetectionLegacy::performThresholdAdaptiveSAT(const cv::Mat& gray, cv::Mat& binary) const
{
    CV_Assert(gray.type() == CV_8UC1);

    constexpr int R = 7;
    constexpr int WIN = 2 * R + 1;
    constexpr uint32_t area = WIN * WIN; // 225

    constexpr uint32_t FACTOR_Q16 = (uint32_t)std::lround(0.9 * 65536.0);
    constexpr uint32_t LHS_SCALE  = area * 65536u; // 14,745,600

    cv::Mat padded;
    cv::copyMakeBorder(gray, padded, R, R, R, R, cv::BORDER_REPLICATE);

    cv::Mat ii;
    cv::integral(padded, ii, CV_32S);

    binary.create(gray.size(), CV_8UC1);

    const int rows = gray.rows;
    const int cols = gray.cols;

    for (int y = 0; y < rows; ++y) {
        const uint8_t* gRow = gray.ptr<uint8_t>(y);
        uint8_t* bRow = binary.ptr<uint8_t>(y);

        const int py1 = y;
        const int py2 = y + WIN;

        const int32_t* row1 = ii.ptr<int32_t>(py1);
        const int32_t* row2 = ii.ptr<int32_t>(py2);

        for (int x = 0; x < cols; ++x) {
            const int px1 = x;
            const int px2 = x + WIN;

            const int32_t A = row1[px1];
            const int32_t B = row1[px2];
            const int32_t C = row2[px1];
            const int32_t D = row2[px2];

            const uint32_t sum = (uint32_t)(D - B - C + A);

            const uint32_t lhs = (uint32_t)gRow[x] * LHS_SCALE;  // fits in u32
            const uint32_t rhs = sum * FACTOR_Q16;               // fits in u32

            bRow[x] = (lhs < rhs) ? 255 : 0;
        }
    }
}

void DetectionLegacy::performThresholdOTSU(const cv::Mat& gray, cv::Mat& binary) const {
    cv::threshold(gray, binary, 60, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
}
//only little endian (arm)
void DetectionLegacy::performThresholdAdaptiveSAT_NEON4(const cv::Mat& gray, cv::Mat& binary) const
{
    CV_Assert(gray.type() == CV_8UC1);

    constexpr int R = 7;
    constexpr int WIN = 2 * R + 1;
    constexpr int area = WIN * WIN; // 225

    constexpr uint32_t FACTOR_Q16 = (uint32_t)std::lround(0.9 * 65536.0); // ~58982
    constexpr uint32_t LHS_SCALE  = (uint32_t)(area * 65536);            // 14,745,600

    const int rows = gray.rows;
    const int cols = gray.cols;

    binary.create(gray.size(), CV_8UC1);

    cv::Mat padded;
    cv::copyMakeBorder(gray, padded, R, R, R, R, cv::BORDER_REPLICATE);

    cv::Mat ii;
    cv::integral(padded, ii, CV_32S);

    // Vector constants (unsigned)
    const uint32x4_t vLHS = vdupq_n_u32(LHS_SCALE);
    const uint32x4_t vF   = vdupq_n_u32(FACTOR_Q16);

    for (int y = 0; y < rows; ++y) {
        const uint8_t* gRow = gray.ptr<uint8_t>(y);
        uint8_t* bRow = binary.ptr<uint8_t>(y);

        const int y1 = y;
        const int y2 = y + WIN;

        const int32_t* row1 = ii.ptr<int32_t>(y1);
        const int32_t* row2 = ii.ptr<int32_t>(y2);

        int x = 0;

        // 4 pixels per iteration
        for (; x <= cols - 4; x += 4) {
            const int x1 = x;
            const int x2 = x + WIN;

            // Load 4 grayscale pixels (bytes)
            uint32_t pix4;
            std::memcpy(&pix4, gRow + x, sizeof(pix4)); // unaligned-safe load

            // Put bytes into lanes 0..3, widen to u32 lanes 0..3
            uint8x8_t  pix_u8  = vcreate_u8((uint64_t)pix4);
            uint16x8_t pix_u16 = vmovl_u8(pix_u8);
            uint32x4_t pix_u32 = vmovl_u16(vget_low_u16(pix_u16)); // lanes 0..3 valid

            // Load A,B,C,D for 4 lanes
            int32x4_t A = vld1q_s32(row1 + x1);
            int32x4_t B = vld1q_s32(row1 + x2);
            int32x4_t C = vld1q_s32(row2 + x1);
            int32x4_t D = vld1q_s32(row2 + x2);

            // sum = D - B - C + A  (should be non-negative)
            int32x4_t sum = vaddq_s32(vsubq_s32(vsubq_s32(D, B), C), A);

            // Treat sum as unsigned (we only care about its magnitude)
            uint32x4_t sum_u = vreinterpretq_u32_s32(sum);

            // rhs = sum * FACTOR_Q16 (u32)
            uint32x4_t rhs_u = vmulq_u32(sum_u, vF);

            // lhs = pix * LHS_SCALE (u32)
            uint32x4_t lhs_u = vmulq_u32(pix_u32, vLHS);

            // mask = (lhs < rhs) <=> (rhs > lhs), unsigned compare
            uint32x4_t m = vcgtq_u32(rhs_u, lhs_u);

            // Narrow mask to 4 bytes: 0xFF or 0x00 per pixel
            uint16x4_t m16 = vmovn_u32(m);                    // 4x16-bit
            uint8x8_t  m8  = vmovn_u16(vcombine_u16(m16, m16)); // 8x8-bit, low 4 are what we want

            // Store low 4 bytes
            uint32_t out4 = vget_lane_u32(vreinterpret_u32_u8(m8), 0);
            std::memcpy(bRow + x, &out4, sizeof(out4));
        }

        // Scalar tail (correct, no signed overflow UB)
        for (; x < cols; ++x) {
            const int x1 = x;
            const int x2 = x + WIN;

            const int32_t A = row1[x1];
            const int32_t B = row1[x2];
            const int32_t C = row2[x1];
            const int32_t D = row2[x2];

            const uint32_t sum = (uint32_t)(D - B - C + A);

            const uint32_t lhs = (uint32_t)gRow[x] * LHS_SCALE;
            const uint32_t rhs = sum * FACTOR_Q16;

            bRow[x] = (lhs < rhs) ? 255 : 0;
        }
    }
}

// Consumes binary
void DetectionLegacy::findContours(cv::Mat& binary,
                                   std::vector<std::vector<cv::Point>>& contours,
                                   int minContourPointsAllowed) const {
                                    
    
    auto t0 = std::chrono::high_resolution_clock::now();
    if (binary.empty()) return;
    
    const int minPoints = minContourPointsAllowed * 4;
    std::vector<std::vector<cv::Point>> all;
    
    cv::findContours(binary, all, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
    
    contours.clear();
    contours.reserve(all.size());
    
    for (auto& c : all) {
        if ((int)c.size() > minPoints) {
            contours.push_back(std::move(c));
        }
    }
    
    std::sort(contours.begin(), contours.end(), 
              [](const auto& a, const auto& b) { return a.size() > b.size(); });
    auto t1 = std::chrono::high_resolution_clock::now();
    //log every 20 frames
    if (frame_idx % 20 == 0) {
auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        perfLog_contours << "DetectionLegacy::findContours took " << us << " us for " << contours.size() << " contours\n";
    }
    
}

void DetectionLegacy::refineCorners(const cv::Mat& gray, Marker& marker) const {
    cv::TermCriteria term(cv::TermCriteria::MAX_ITER | cv::TermCriteria::EPS, 30, 0.01);
    cv::cornerSubPix(gray, marker.points, cv::Size(5,5), cv::Size(-1,-1), term);
    marker.calculateGoodFeaturesToTrack();
    cv::cornerSubPix(gray, marker.calculatedGoodFeaturesToTrack, cv::Size(3,3), cv::Size(-1,-1), term);
}


void DetectionLegacy::findCandidates(const std::vector<std::vector<cv::Point> > &contours,
                                   std::vector<Marker> &detectedMarkers) const
{
    StopwatchGuard s( m_stopwatch, "DetectionLegacy::findCandidates");
    std::vector<cv::Point> approxCurve;
    std::vector<Marker>
    possibleMarkers;

    {
        for (size_t i=0; i<contours.size(); i++) {
            cv::Mat contour(contours[i]);
            double eps = contours[i].size() * 0.05;
            cv::approxPolyDP(contour, approxCurve, eps, true);

            // We interested only in polygons that contains only four points

            if (approxCurve.size() != 4) {
                continue;
            }

            // And they have to be convex
            if (!cv::isContourConvex(cv::Mat(approxCurve))) {
                continue;
            }

            // Ensure that the distance between consecutive points is large enough
            float minDist = std::numeric_limits<float>::max();

            for (int i = 0; i < 4; i++) {
                cv::Point side = approxCurve[i] - approxCurve[(i+1)%4];
                float squaredSideLength = (float) side.dot(side);
                minDist = std::min(minDist, squaredSideLength);
            }

            // Check that distance is not very small
            if (minDist < config->minContourLengthAllowed) {
                continue;
            }

            // All tests are passed. Save marker candidate:
            Marker m;

            for (int i = 0; i<4; i++) {
                m.points.push_back( cv::Point2f(approxCurve[i].x,approxCurve[i].y) );
            }

            // Sort the points in anti-clockwise order
            // Trace a line between the first and second point.
            // If the third point is at the right side, then the points are anti-clockwise
            cv::Point v1 = m.points[1] - m.points[0];
            cv::Point v2 = m.points[2] - m.points[0];

            double o = (v1.x * v2.y) - (v1.y * v2.x);

            if (o < 0.0) {
                //if the third point is in the left side, then sort in anti-clockwise order
                std::swap(m.points[1], m.points[3]);
            }

            possibleMarkers.push_back(m);

        }
    }

    // Remove these elements which corners are too close to each other.
    // First detect candidates for removal:
    std::vector< std::pair<int,int> > tooNearCandidates;
    for (size_t i=0;i<possibleMarkers.size();i++) {
        const Marker& m1 = possibleMarkers[i];

        //calculate the average distance of each corner to the nearest corner of the other marker candidate
        for (size_t j=i+1;j<possibleMarkers.size();j++) {
            const Marker& m2 = possibleMarkers[j];

            float distSquared = 0;

            for (int c = 0; c < 4; c++) {
                cv::Point v = m1.points[c] - m2.points[c];
                distSquared += v.dot(v);
            }

            distSquared /= 4;

            // Thies: hier stand zuvor eine 100
            if (distSquared < 10) {
                tooNearCandidates.push_back(std::pair<int,int>(i,j));
            }
        }
    }


    // Mark for removal the element of the pair with smaller perimeter
    std::vector<bool> removalMask (possibleMarkers.size(), false);

    for (size_t i=0; i<tooNearCandidates.size(); i++) {

        float p1 = (float)cv::arcLength(cv::Mat(possibleMarkers[tooNearCandidates[i].first].points), true);
        float p2 = (float)cv::arcLength(cv::Mat(possibleMarkers[tooNearCandidates[i].second].points), true);

        size_t removalIndex;
        if (p1 > p2)
        removalIndex = tooNearCandidates[i].second;
        else
        removalIndex = tooNearCandidates[i].first;

        removalMask[removalIndex] = true;
    }

    // Return candidates
    detectedMarkers.clear();
    for (size_t i=0;i<possibleMarkers.size();i++) {
        if (!removalMask[i]) {
            detectedMarkers.push_back(possibleMarkers[i]);
        }
    }
}


// Recognize/decode one quad. NO config, NO sizes here.
bool DetectionLegacy::recognizeMarker(const cv::Mat& gray, Marker& marker) const {
    cv::Point2f src[4] = { marker.points[0], marker.points[1], marker.points[2], marker.points[3] };
    cv::Mat H = cv::getPerspectiveTransform(src, markerCorners2d_);

    cv::Mat canonical;
    cv::warpPerspective(gray, canonical, H, markerSizePixels_, cv::INTER_NEAREST);

    int nRotations = 0;
    cv::Mat bitCode = Marker::getMarkerBitCode(canonical, nRotations);
    if (bitCode.empty()) return false;

    marker.bitCode = bitCode;
    marker.id = Marker::getMarkerId(bitCode);

    // keep consistent corner order across rotations
    std::rotate(marker.points.begin(), marker.points.begin() + 4 - nRotations, marker.points.end());
    marker.tracked = false;
    return true;
}

bool DetectionLegacy::findMarkers(const cv::Mat& scaled,
                                 const cv::Mat &grayscale,
                                 std::vector<Marker>& detectedMarkers,
                                 cv::Point2f deviation,
                                 bool skip3d,
                                 ThresholdMethod method,
                                 double scalingFactor )
{
    std::vector<Marker> candidateMarkers;

    //LOG(debug) << "** Scaling Factor is " << scalingFactor << "\n";
    StopwatchGuard s( m_stopwatch, "DetectionLegacy::findMarkers");
    //// pixels processed this frame
    const int proc_pixels = scaled.cols * scaled.rows;
    //// --- timing tresholds 
    cv::Mat grayScaled;
    if (scaled.channels() == 3) {
        cv::cvtColor(scaled, grayScaled, cv::COLOR_BGR2GRAY);
    } else {
        grayScaled = scaled;
    }
    // std::cerr
    // << "[DBG] scaled: " << scaled.cols << "x" << scaled.rows
    // << " ch=" << scaled.channels()
    // << "\n";

    auto t0 = std::chrono::high_resolution_clock::now();

    // Make it binary
    cv::Mat thresholdImg;
    const char* which;
    
    switch (method) {
    case ThresholdMethod::OTSU:{
    //std::cerr << "[TRACE] entering OTSU branch\n";

    this->performThresholdOTSU(grayScaled, thresholdImg);

    //std::cerr
       // << "[TRACE] after performThresholdOTSU: "
        //<< "thresholdImg=" << thresholdImg.cols
       // << "x" << thresholdImg.rows
       // << " ch=" << thresholdImg.channels()
        //<< " empty=" << thresholdImg.empty()
        //<< " type=" << thresholdImg.type()
        //<< "\n";
       // which = "otsu";

    break;
}
    
     case ThresholdMethod::v1:
        performThresholdAdaptiveSAT_NEON16(scaled, thresholdImg ); 
       // which = "v1";
        break;
    case ThresholdMethod::v2:
        performThreshold(scaled, thresholdImg ); 
       // which = "v2";
        break;
    case ThresholdMethod::v3:
        performThresholdAdaptiveSAT_NEON8(scaled, thresholdImg ); 
       // which = "v3";
        break;
    case ThresholdMethod::v4:
        this->adaptiveThresholdMeanFast(scaled,
                thresholdImg,
                255,
                15,
                0.9);
       // which = "v4";
        break;
    case ThresholdMethod::v5:
        performThresholdAdaptiveSAT(scaled, thresholdImg ); 
       // which = "v5";
        break;
    case ThresholdMethod::v6:
        performThresholdAdaptiveSAT_NEON4(scaled, thresholdImg ); 
       // which = "v6";
        break;
    default:
        this->performThresholdAdaptiveSAT_NEON4(grayScaled , thresholdImg);
       // which = "v6";
        break;
    }

    
    

    if (thresholdImg.empty()) {
   // std::cerr << "[ERROR] thresholdImg is empty after method " << which << "\n";
}

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms_threshold = std::chrono::duration<double, std::milli>(t1 - t0).count();

    static size_t frame_idx = 0;

    // Log only every N frames (e.g. every 20)
    constexpr size_t LOG_EVERY = 20;

  /*  if (frame_idx % LOG_EVERY == 0) {
        //static std::ofstream log("threshold_times.csv", std::ios::app);
        perfLog << proc_pixels << "," << ms_threshold << "\n";
        // flush occasionally
         if (frame_idx % (LOG_EVERY * 50) == 0) perfLog.flush();
    } */


    interruptionPoint(); // ### INTERRUPTION POSSIBILITY ###
    //measure time after thresholding
    auto t_rest = std::chrono::high_resolution_clock::now();
    // Detect contours
    std::vector< std::vector<cv::Point> > contours;
    // Formerly here was scalingFactor * m_minMarkerSizeInPixels. But this does not make any sense...
    // Because we need markers of a specific number of pixels anyway.
    findContours(thresholdImg, contours, config->minMarkerSizeInPixels  ); //frame.cols / 10); // scalingFactor *
    
    interruptionPoint(); // ### INTERRUPTION POSSIBILITY ###

    // Find closed contours that can be approximated with 4 points
    findCandidates(contours, candidateMarkers);

    interruptionPoint(); // ### INTERRUPTION POSSIBILITY ###

    for (size_t i = 0; i < candidateMarkers.size(); ++i) {
        Marker marker = candidateMarkers[i];
        if (recognizeMarker(scaled, marker)) {
            // If ROI was used, transform markers to original image size
            refineCorners(scaled, marker);
            for (unsigned int p = 0; p < marker.points.size(); ++p) {
                marker.points[p] += deviation;
            }
            for (unsigned int p = 0; p < marker.calculatedGoodFeaturesToTrack.size(); ++p) {
                marker.calculatedGoodFeaturesToTrack[p] += deviation;
            }
            detectedMarkers.push_back(marker);
        }
    }
    auto t_rest_end = std::chrono::high_resolution_clock::now();
    //save t_rest_end - t_rest
    double t_rest_duration = std::chrono::duration<double, std::milli>(t_rest_end - t_rest).count();
    //log t_rest_duration every 20 frames
    //std::cout << frame_idx << std::endl;
    if (frame_idx % 20 == 0) {
    perfLog << proc_pixels << "," << ms_threshold << ",";

    perfLog << "DetectionLegacy::findMarkers rest time (ms): " << t_rest_duration << ",";
        perfLog << "candidates Size: " << candidateMarkers.size() << ",";
        perfLog << "detectedMarkers Size: " << detectedMarkers.size() << "\n";
}
    //flush occasionally
    if (frame_idx % (LOG_EVERY * 50) == 0) perfLog.flush();
    frame_idx++;

    return !detectedMarkers.empty();
}

bool DetectionLegacy::findMarkersWithPrior(const cv::Mat &scaled,
                                         const cv::Mat &grayscale,
                                         float roiScale,
                                         Marker &priorMarker,
                                         std::vector<Marker> &detectedMarkers,
                                         ThresholdMethod method)
{
    StopwatchGuard s( m_stopwatch, "DetectionLegacy::findMarkerWithPrior");

    float scale = (float)grayscale.cols / (float)scaled.cols;
    std::vector<cv::Point2f> upscaledPoints;
    for (unsigned int p = 0; p < priorMarker.points.size(); ++p) {
        upscaledPoints.push_back(priorMarker.points[p] * scale);
    }
    // Apply ROI to original image
    cv::Rect roi = cv::boundingRect(upscaledPoints);
    roi += cv::Point(-20, -20);
    roi += cv::Size(40, 40);

    roi.x = std::max(roi.x, 0);
    roi.y = std::max(roi.y, 0);
    roi.width = std::min(roi.width, grayscale.cols - roi.x);
    roi.height = std::min(roi.height, grayscale.rows - roi.y);

    //    //skip stupid sizes
    if ((roi.width <= 10.0) || (roi.height <= 10.0)){
        return false; //nothing found
    }

    // Find markers in extended ROI
    cv::Mat roiImg(grayscale, roi);
    if (roiImg.type() != CV_8UC1) {
        prepareImage(roiImg, roiImg);
    }

    return findMarkers(roiImg, grayscale, detectedMarkers, cv::Point2f(roi.x, roi.y), false, method); //use  otsu 6th
}
