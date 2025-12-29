/*****************************************************************************
*   Marker.hpp
*   Example_MarkerBasedAR
******************************************************************************
*   by Khvedchenia Ievgen, 5th Dec 2012
*   http://computer-vision-talks.com
******************************************************************************
*   Ch2 of the book "Mastering OpenCV with Practical Computer Vision Projects"
*   Copyright Packt Publishing 2012.
*   http://www.packtpub.com/cool-projects-with-opencv/book
*****************************************************************************/

#ifndef KALMANMARKER_H
#define KALMANMARKER_H
#include "Marker.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/tracking.hpp>



/**
 * This class represents a kalman tracked marker
 */
class KalmanMarker
{  
public:
  KalmanMarker(Marker m, boost::posix_time::ptime timestamp);
  KalmanMarker();

  //return true if the marker has been seen recently
  bool is_alive();

  void next_frame(boost::posix_time::ptime frame);
  int get_id(){ return id; }

  Marker get_marker(boost::posix_time::ptime timestamp);
  void update_marker(Marker _marker, boost::posix_time::ptime timestamp);

  cv::Point2f get_current_position(){ return coord; }
private:
  Marker marker;
  void set_kalman_coefficients(double p, double m, double e);

  int id;
  cv::Point2f coord;

  boost::posix_time::ptime timestamp_last_updated;
  boost::posix_time::ptime timestamp_last_frame;

  ///kalman filter stuff
  double options_kalman_process_noise_cov;
  double options_kalman_measurement_noise_cov;
  double options_kalman_error_cov;

  //void set(double x, double y, double w, double h);
  cv::KalmanFilter *kalman_filter = nullptr;
  bool marker_updated = false;
  int not_seen_counter = 0;


};

#endif //KALMANMARKER_H
