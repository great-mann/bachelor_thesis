/*****************************************************************************
*   CameraCalibration.hpp
*   Example_MarkerBasedAR
******************************************************************************
*   by Khvedchenia Ievgen, 5th Dec 2012
*   http://computer-vision-talks.com
******************************************************************************
*   Ch2 of the book "Mastering OpenCV with Practical Computer Vision Projects"
*   Copyright Packt Publishing 2012.
*   http://www.packtpub.com/cool-projects-with-opencv/book
*****************************************************************************/

#ifndef CAMERA_CALIBRATION_H
#define CAMERA_CALIBRATION_H

////////////////////////////////////////////////////////////////////
#include <opencv2/opencv.hpp>

/**
 * A camera calibraiton class that stores intrinsic matrix
 * and distorsion vector.
 */
class CameraCalibration
{
public:
  CameraCalibration();
  CameraCalibration(float fx, float fy, float cx, float cy);
  CameraCalibration(float fx, float fy, float cx, float cy, float distorsionCoeff[4]);
  
  void getMatrix34(float cparam[3][4]) const;

  const cv::Mat& getIntrinsic() const;
  const cv::Mat&  getDistorsion() const;
  
private:
  cv::Mat m_intrinsic;
  cv::Mat m_distorsion;
};

#endif // CAMERA_CALIBRATION_H