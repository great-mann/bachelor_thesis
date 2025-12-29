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

#ifndef MARKER_H
#define MARKER_H

////////////////////////////////////////////////////////////////////
// Standard includes:
#include <vector>
#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

////////////////////////////////////////////////////////////////////


/**
 * This class represents a marker
 */
class Marker
{  
public:
  Marker(int _id=-1);
  
  friend bool operator<(const Marker &M1,const Marker&M2);
  friend std::ostream & operator<<(std::ostream &str,const Marker &M);

  
  static cv::Mat rotate(cv::Mat  in);
  static int hammDistMarker(cv::Mat bits);
  static cv::Mat getMarkerBitCode(cv::Mat &in,int &nRotations,unsigned int regions=7);
  static int mat2id(const cv::Mat &bits);
  static int getMarkerId(cv::Mat &bits);
  void translate(cv::Point2f dist);

  cv::Point2f get_center();

  // Id of  the marker
  int id;
  cv::Mat bitCode;
  
  cv::Mat perspectiveTransform;
  
  bool tracked; // if true, this marker has been tracked and not detected.
  
  bool hasCameraTransformation; // if true, this marker has been globally calculated
  
  
  cv::Point2f m_markerCorners2d[4];
  
  // Size of the marker
  cv::Size2f size;
  
  
  // Marker transformation with regards to the camera
  cv::Mat transformation;
  // Global transformation of the camera
  cv::Mat cameraTransformation;
  
  std::vector< cv::Point2f > calculatedGoodFeaturesToTrack;
  std::vector< cv::Point3f > rectifiedGoodFeaturesToTrack;
  
  std::string source;

  // Quality in [0,1]
  float quality; 
  
  std::vector<cv::Point2f> points;
  std::vector<cv::Point2f> tracked_points;
  // Helper function to draw the marker contour over the image
  void drawContour(cv::Mat& image, cv::Scalar color = cv::Scalar(0,250,0,0), std::string text="");
  void drawCross(cv::Mat& image, cv::Scalar color = cv::Scalar(0,250,0,0), std::string text="");
  void drawText(cv::Mat& image, cv::Point2f pos, cv::Scalar color, std::string text="");

  // Returns a vector of good feature positions for tracking
  std::vector< cv::Point2f > getGoodFeaturesToTrack( cv::Mat& );
  void calculateGoodFeaturesToTrack();
  
  int getImageSize() const;
};

#endif //MARKER_H
