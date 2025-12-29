#ifndef __MedianFlow_h_included__
#define __MedianFlow_h_included__

#include <opencv2/opencv.hpp>
#include <opencv2/ml/ml.hpp>
#include "Marker.h"
#include "Stopwatch.h"
#include <vector>

class MedianFlow
{
public:
	MedianFlow();
	~MedianFlow();

	void beginTracking( const cv::Mat& lastFrame, const cv::Mat& newFrame );
	bool track( Marker& marker );
	void endTracking();

	void printTimings( std::ostream& );

protected:
	void allocateOnDemand( cv::Mat& img, cv::Size size, int type );
	double euclid_dist(const cv::Point2f* point1, const cv::Point2f* point2);
	void pairwise_dist(const cv::Point2f* features, double *edist, int npoin);
	void bbox_move(const cv::Point2f* prev_feat, const cv::Point2f* curr_feat, const int npoin, double &xmean, double &ymean);
	void fb_filter(std::vector<cv::Point2f>& prev_features, std::vector<cv::Point2f>& backward_features, std::vector<cv::Point2f>& curr_feat, std::vector<int>& fb_pass);
	void ncc_filter(cv::Mat frame1, cv::Mat frame2, std::vector<cv::Point2f>& prev_feat, std::vector<cv::Point2f>& curr_feat, int method,  std::vector<int>& ncc_pass);
	cv::Rect getBBox( std::vector<cv::Point2f>& points );

private:
	cv::Mat  frame2_1C;
	cv::Mat  frame1_1C;
	cv::Size frame_size;
	Stopwatch m_stopwatch;
	cv::Mat  m_features;
	bool	 m_debugging;
};

#endif // __MedianFlow_h_included__
