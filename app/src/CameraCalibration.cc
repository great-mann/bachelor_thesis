/*****************************************************************************
*   CameraCalibration.cpp
*   Example_MarkerBasedAR
******************************************************************************
*   by Khvedchenia Ievgen, 5th Dec 2012
*   http://computer-vision-talks.com
******************************************************************************
*   Ch2 of the book "Mastering OpenCV with Practical Computer Vision Projects"
*   Copyright Packt Publishing 2012.
*   http://www.packtpub.com/cool-projects-with-opencv/book
*****************************************************************************/

#include "CameraCalibration.h"


CameraCalibration::CameraCalibration()
{
  
}

CameraCalibration::CameraCalibration(float fx, float fy, float cx, float cy)
{
  m_intrinsic = cv::Mat::zeros(3,3,CV_32F);
  m_distorsion = cv::Mat::zeros(1,4,CV_32F);
    
  m_intrinsic.at<float>(0,0) = fx;
  m_intrinsic.at<float>(1,1) = fy;
  m_intrinsic.at<float>(0,2) = cx;
  m_intrinsic.at<float>(1,2) = cy;
  
}

CameraCalibration::CameraCalibration(float fx, float fy, float cx, float cy, float distorsionCoeff[4])
{
  m_intrinsic = cv::Mat::zeros(3,3,CV_32F);
  m_distorsion = cv::Mat::zeros(1,4,CV_32F);
    
  m_intrinsic.at<float>(0,0) = fx;
  m_intrinsic.at<float>(1,1) = fy;
  m_intrinsic.at<float>(0,2) = cx;
  m_intrinsic.at<float>(1,2) = cy;
  
  for (int i=0; i<4; i++)
    m_distorsion.at<float>(0,i) = distorsionCoeff[i];
}

void CameraCalibration::getMatrix34(float cparam[3][4]) const
{
  for (int j=0; j<3; j++)
    for (int i=0; i<3; i++)
      cparam[i][j] = m_intrinsic.at<float>(i,j);
  
  for (int i=0; i<4; i++)
    cparam[3][i] = m_distorsion.at<float>(0,i);
}

const cv::Mat& CameraCalibration::getIntrinsic() const
{
  return m_intrinsic;
}

const cv::Mat&  CameraCalibration::getDistorsion() const
{
  return m_distorsion;
}
