#include "MedianFlow.h"

#include <stdio.h>

using namespace cv;
using namespace std;

MedianFlow
::MedianFlow() 
:	m_debugging( false )
{
}

MedianFlow
::~MedianFlow() {
}

void
MedianFlow
::beginTracking( const cv::Mat& lastFrameMat, const cv::Mat& newFrameMat )
{
	//cv::namedWindow("OldFrame");
	//cv::namedWindow("NewFrame");
	StopwatchGuard s( m_stopwatch, "MedianFlow::beginTracking");
	frame_size.width = newFrameMat.cols;
	frame_size.height = newFrameMat.rows;
	
	/**
	if( !frame1_1C.empty() )
		cv::imshow( "OldFrame", frame1_1C );
	if( !frame2_1C.empty() )
		cv::imshow( "NewFrame", frame2_1C );
	**/

	if( !frame2_1C.empty() && lastFrameMat.size == newFrameMat.size )
		frame1_1C = frame2_1C.clone();
	else
		cvtColor(lastFrameMat, frame1_1C, cv::COLOR_BGR2GRAY);

	cvtColor(newFrameMat, frame2_1C, cv::COLOR_BGR2GRAY);
	
	if( m_debugging )
		m_features = lastFrameMat.clone();
}

void
MedianFlow
::endTracking() {
	if( m_debugging ) {
		cv::namedWindow("Features");
		cv::imshow("Features", m_features);
		cv::waitKey(30);
	}
}

bool
MedianFlow
::track( Marker& marker ) {
	StopwatchGuard s( m_stopwatch, "MedianFlow::track");
	//cvNamedWindow("MedianFlow");
	cv::Rect previousBoundingBox = cv::boundingRect(marker.points);
	// Add some "safety zone"
	previousBoundingBox.x -= 10;
	previousBoundingBox.y -= 10;
	previousBoundingBox.width += 20;
	previousBoundingBox.height += 20;

	//double start, end,t1, t2;	
	int pcount;
	int winsize = 30;

	/* The i-th element of this array will be non-zero if and only if the i-th feature of
	 * frame 1 was found in frame 2.
	 */
	vector<uchar> optical_flow_found_feature;
	vector<uchar> optical_flow_found_feature2;
	vector<float> optical_flow_feature_error;

	/* This array will contain the locations of the points from frame 1 in frame 2. */
	vector<cv::Point2f> frame1_features;
	vector<cv::Point2f> frame2_features;
	vector<cv::Point2f> FB_features;
	// Alternatively we scan the image for good features to track:
	{
		StopwatchGuard s( m_stopwatch, "MedianFlow::goodFeaturesToTrack");
		marker.calculateGoodFeaturesToTrack( );
        frame1_features = marker.calculatedGoodFeaturesToTrack;
		//cv::goodFeaturesToTrack( region, corners, 30, 0.01, 5 );//, mask );//, mask, 3, false, 0.04 );
	}
	//fprintf(stderr, "Good features found: %d\n",frame1_features.size() );
	if( m_debugging ) {
		
		vector<cv::Point2f>::iterator itt = frame1_features.begin();
		while( itt != frame1_features.end() ) {
			circle( m_features, *itt, 4, Scalar( 255, 255, 255 ), 2, 8 );
			++itt;
		}
		
	}

	/* Pyramidal Lucas Kanade Optical Flow! */

	/* This is the window size to use to avoid the aperture problem (see slide "Optical Flow: Overview"). */
	cv::Size optical_flow_window = cv::Size(10,10);
	cv::Rect searchRegion; 
	{
		StopwatchGuard s( m_stopwatch, "MedianFlow::opticalFlow");
		cv::TermCriteria optical_flow_termination_criteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 20, 0.3);


		// Determine region to search for
		searchRegion = getBBox(marker.points);
		
		searchRegion.x -= 1*searchRegion.width;
		searchRegion.y -= 1*searchRegion.height;
		searchRegion.width = 3 * searchRegion.width;
		searchRegion.height = 3 * searchRegion.height;
		// We have to clamp the searchRegion to the frame...
		if( searchRegion.x < 0 ) { searchRegion.width += searchRegion.x; searchRegion.x = 0; }
		if( searchRegion.y < 0 ) { searchRegion.height += searchRegion.y; searchRegion.y = 0; }
		if( searchRegion.x > frame_size.width ) return false;
		if( searchRegion.y > frame_size.height ) return false;
		if( searchRegion.x + searchRegion.width > frame_size.width ) { searchRegion.width = frame_size.width - searchRegion.x; }
		if( searchRegion.y + searchRegion.height > frame_size.height ) { searchRegion.height = frame_size.height - searchRegion.y; }
		if( searchRegion.width <= 0 ) return false;
		if( searchRegion.height <= 0 ) return false;

		cv::Mat region_1( frame1_1C, searchRegion );
		cv::Mat region_2( frame2_1C, searchRegion );

		// Fix features...
		vector<cv::Point2f> frame1_features_regions(frame1_features.size());
		for( int g = 0; g < frame1_features.size(); ++g ) {
			frame1_features_regions[g].x = frame1_features[g].x - searchRegion.x;
			frame1_features_regions[g].y = frame1_features[g].y - searchRegion.y;
		}

		cv::calcOpticalFlowPyrLK(region_1, region_2, frame1_features_regions, frame2_features, optical_flow_found_feature, optical_flow_feature_error, optical_flow_window, 5, optical_flow_termination_criteria, 0 );
		cv::calcOpticalFlowPyrLK(region_2, region_1, frame2_features, FB_features, optical_flow_found_feature2, optical_flow_feature_error, optical_flow_window, 5, optical_flow_termination_criteria, 0 );

		// Fix features
		vector<cv::Point2f>::iterator it = frame2_features.begin();
		while( it != frame2_features.end() ) {
			it->x = it->x + searchRegion.x;
			it->y = it->y + searchRegion.y;
			++it;
		}

		it = FB_features.begin();
		while( it != FB_features.end() ) {
			it->x = it->x + searchRegion.x;
			it->y = it->y + searchRegion.y;
			++it;
		}

		if( m_debugging ) {
			// Draw features 
			Scalar col = Scalar( 255, 255, 255 );
			rectangle( m_features, cv::Point( searchRegion.x, searchRegion.y ), cv::Point( searchRegion.x + searchRegion.width, searchRegion.y + searchRegion.height ), col, 1, 8 );
			vector<cv::Point2f>::iterator it = frame2_features.begin();
			while( it != frame2_features.end() ) {
				circle( m_features, *it, 4, Scalar( 0, 128, 0 ), 3, 8 );
				++it;
			}
			
			it = FB_features.begin();
			while( it != FB_features.end() ) {
				circle( m_features, *it, 2, Scalar( 128, 0, 0 ), 2, 8 );
				++it;
			}
			
		}
	}
	vector<int> fb_pass(frame1_features.size());
	vector<int> ncc_pass(frame1_features.size());
	//filter cascade
	{
		StopwatchGuard s( m_stopwatch, "MedianFlow::filter");
		fb_filter(frame1_features, FB_features, frame2_features, fb_pass);
		ncc_filter(frame1_1C,frame2_1C,frame1_features,frame2_features,cv::TM_CCOEFF_NORMED, ncc_pass);
	}

	pcount = 0;		   
	for (int i = 0; i < frame1_features.size(); i++) {
		if (ncc_pass[i] && fb_pass[i]) {
			pcount++;
		}
	}
	//fprintf(stderr, "pcount: %d\n", pcount);

	vector<cv::Point2f> curr_features2(pcount), prev_features2(pcount);
	int j = 0;
	for (int i = 0; i < frame1_features.size(); i++) {
			if( m_debugging && fb_pass[i]) {
				// Draw features 
				circle( m_features, frame2_features[i], 2, Scalar( 0, 255, 0 ), 3, 8 );
			}
		if (ncc_pass[i] && fb_pass[i]) {
			curr_features2[j] = frame2_features[i];
			prev_features2[j] = frame1_features[i];

			j++;
		}
	}

	if( pcount < 8 ) {
		marker.tracked_points.clear();
		marker.tracked = false;
		if( m_debugging ) {
			// Draw features 
			Scalar col = Scalar( 0, 0, 255 );
			rectangle( m_features, cv::Point( searchRegion.x, searchRegion.y ), cv::Point( searchRegion.x + searchRegion.width, searchRegion.y + searchRegion.height ), col, 1, 8 );
		}
		return false;
	}
	pcount = j;

	cv::Mat transform; 
	{
		StopwatchGuard s( m_stopwatch, "MedianFlow::findHomography");
		transform = findHomography( prev_features2, curr_features2, cv::RANSAC, 3 );
		if( transform.cols == 0 )
			return false;
	}
	
	std::vector< cv::Point2f > target;
	perspectiveTransform( marker.points, target, transform );

	// Test the transformed points if they make sense...

	// Approximate to a polygon
	double eps = target.size() * 0.05;
	std::vector<cv::Point2f> approxCurve;
	cv::approxPolyDP(cv::Mat(target), approxCurve, eps, true);

	// We interested only in polygons that contains only four points
	if (approxCurve.size() != 4) {
		//std::cout << " !=4 ";
		//fprintf(stderr, "----------------- not four -------------------------\n");
		if( m_debugging ) {
			// Draw features 
			Scalar col = Scalar( 0, 0, 255 );
			rectangle( m_features, cv::Point( searchRegion.x, searchRegion.y ), cv::Point( searchRegion.x + searchRegion.width, searchRegion.y + searchRegion.height ), col, 1, 8 );
		}
		return false;
	}

	// And they have to be convex
	if (!cv::isContourConvex(cv::Mat(approxCurve))) {
		//std::cout << " !O ";
		//fprintf(stderr, "----------------- not convex -------------------------\n");
		if( m_debugging ) {
			// Draw features 
			Scalar col = Scalar( 0, 0, 255 );
			rectangle( m_features, cv::Point( searchRegion.x, searchRegion.y ), cv::Point( searchRegion.x + searchRegion.width, searchRegion.y + searchRegion.height ), col, 1, 8 );
		}
		return false;
	}
    
    // And try checking if the sides are parallel
    cv::Vec2f dir1(cv::Vec2f(target[1]) - cv::Vec2f(target[0]));
    cv::Vec2f dir2(cv::Vec2f(target[2]) - cv::Vec2f(target[1]));
    cv::Vec2f dir3(cv::Vec2f(target[3]) - cv::Vec2f(target[2]));
    cv::Vec2f dir4(cv::Vec2f(target[0]) - cv::Vec2f(target[3]));
    float v1 = acos( dir1.dot(dir3) / (cv::norm(dir1) * cv::norm(dir3)) );
    float v2 = acos( dir2.dot(dir4) / (cv::norm(dir2) * cv::norm(dir4)) );
    // ..we don't accept markers with four slightly non-parallel sides (ok, for the moment we are quite nasty)
    if (v1 < 3.0f || v2 < 3.0f) {
        return false;
    }
    // ..and with two strongly non-parallel sides
 //   if (v1 < 2.5f || v2 < 2.5f) {
 //       return false;
 //   }
    if ( m_debugging ) {
        std::cout << "Parallel check:" << "\n";
        std::cout << v1 << "\n";
        std::cout << v2 << "\n";
    }

	marker.tracked_points = target;
	marker.points = target;

	return true;
}

void
MedianFlow
::allocateOnDemand( cv::Mat& img, cv::Size size, int type )
{
	 if (img.empty() || img.size() != size || img.type() != type) {
        img = cv::Mat(size, type);
    }
}

double 
MedianFlow
::euclid_dist(const cv::Point2f* point1, const cv::Point2f* point2) {
	//this function calculates the euclidean distance between 2 points
	double distance, xvec, yvec;
	xvec = point2->x - point1->x;
	yvec = point2->y - point1->y;
	distance = sqrt((xvec * xvec) + (yvec * yvec));
	return distance;
}

void 
MedianFlow
::pairwise_dist(const cv::Point2f* features, double *edist, int npoin) {
	//calculate m x n euclidean pairwise distance matrix.
	for (int i = 0; i < npoin; i++) {
		for (int j = 0; j < npoin; j++) {
			int ind = npoin*i + j;
			edist[ind] = euclid_dist(&features[i],&features[j]);
		}
	}
}
void 
MedianFlow
::ncc_filter(cv::Mat frame1, cv::Mat frame2, std::vector<cv::Point2f>& prev_feat, std::vector<cv::Point2f>& curr_feat, int method, vector<int>& ncc_pass) {
	int filt = prev_feat.size()/2;
	vector<float> ncc_err (prev_feat.size(),0.0);
	cv::Mat rec0;
	cv::Mat rec1;
	cv::Mat res;

	for (int i = 0; i < prev_feat.size(); i++) {
		cv::getRectSubPix( frame1, cv::Size(10, 10), prev_feat[i], rec0  );
		cv::getRectSubPix( frame2, cv::Size(10, 10), curr_feat[i], rec1  );
		cv::matchTemplate( rec0, rec1, res, method );
		ncc_err[i] = ((float *)(res.data))[0]; 
	}
	vector<float> err_copy (ncc_err);
	sort(ncc_err.begin(), ncc_err.end());
	double median = (ncc_err[filt]+ncc_err[filt-1])/2.;
	for(int i = 0; i < prev_feat.size(); i++) {
		if (err_copy[i] > median) {
			ncc_pass[i] = 1;		
		}
		else {
			ncc_pass[i] = 0;
		}
	}	
}

void 
MedianFlow
::fb_filter(std::vector<cv::Point2f>& prev_features, std::vector<cv::Point2f>& backward_features, std::vector<cv::Point2f>& curr_feat, vector<int>& fb_pass) {
	//this function implements forward-backward error filtering
	vector<double> euclidean_dist (prev_features.size(),0.0);
	
	int filt = 3 * prev_features.size()/4;
	for(int i = 0; i < prev_features.size(); i++) {
		euclidean_dist[i] = euclid_dist(&prev_features[i], &backward_features[i]);
	}
	
	vector<double> err_copy (euclidean_dist);
	//use the STL sort algorithm to filter results
	sort(euclidean_dist.begin(), euclidean_dist.end());
	double median = (euclidean_dist[filt]+euclidean_dist[filt-1])/2.;
	for(int i = 0; i < prev_features.size(); i++) {
		if (err_copy[i] < (median)) {
			fb_pass[i] = 1;		
		}
		else {
			fb_pass[i] = 0;
		}
	}
}
void 
MedianFlow
::bbox_move(const cv::Point2f* prev_feat, const cv::Point2f* curr_feat, const int npoin,
				double &xmean, double &ymean) {
	/*Calculate bounding box motion. */
	vector<double> xvec (npoin,0.0);
	vector<double> yvec (npoin,0.0);
	for (int i = 0; i < npoin; i++) {
		xvec[i] = curr_feat[i].x - prev_feat[i].x;
		yvec[i] = curr_feat[i].y - prev_feat[i].y;
	}	
	
	sort(xvec.begin(), xvec.end());
	sort(yvec.begin(), yvec.end());
	
	xmean = xvec[npoin/2];
	ymean = yvec[npoin/2];
}

cv::Rect
MedianFlow
::getBBox( std::vector<cv::Point2f>& points ) {
	cv::Rect rv;

	vector<double> x (points.size(),0.0);
	vector<double> y (points.size(),0.0);
	for (int i = 0; i < points.size(); i++) {
		x[i] = points[i].x;
		y[i] = points[i].y;
	}	
	
	sort(x.begin(), x.end());
	sort(y.begin(), y.end());
	
	rv.x = *x.begin();
	rv.y = *y.begin();
    rv.width = *(x.end()-1) - *x.begin();
	rv.height = *(y.end()-1) - *y.begin();
	return rv;
}

void
MedianFlow
::printTimings( std::ostream& out ) {
	m_stopwatch.print(out);

}
