/*****************************************************************************
*   Marker.cpp
*   Example_MarkerBasedAR
******************************************************************************
*   by Khvedchenia Ievgen, 5th Dec 2012
*   http://computer-vision-talks.com
******************************************************************************
*   Ch2 of the book "Mastering OpenCV with Practical Computer Vision Projects"
*   Copyright Packt Publishing 2012.
*   http://www.packtpub.com/cool-projects-with-opencv/book
*****************************************************************************/

#include "Marker.h"
#include "DebugHelpers.h"
#include "opencv2/imgproc/types_c.h"


Marker::Marker(int _id)
: id(_id),
  tracked(false),
  hasCameraTransformation(false),
  size(cv::Size2f(50,50)),
  quality(0),
  source("unknown")
{
    // TODO: which is correct?
//    m_markerCorners2d[0] = cv::Point2f(0,0);
//	m_markerCorners2d[1] = cv::Point2f(100-1,0);
//	m_markerCorners2d[2] = cv::Point2f(100-1,100-1);
//	m_markerCorners2d[3] = cv::Point2f(0,100-1);
    m_markerCorners2d[0] = cv::Point2f(0,0);
	m_markerCorners2d[1] = cv::Point2f(100,0);
	m_markerCorners2d[2] = cv::Point2f(100,100);
	m_markerCorners2d[3] = cv::Point2f(0,100);
    
    transformation = cv::Mat::zeros(4,4,CV_32F);
    cameraTransformation = cv::Mat::zeros(4,4,CV_32F);
}

bool operator<(const Marker &M1,const Marker&M2)
{
  return M1.quality < M2.quality;
}

int Marker::getImageSize() const {
    if (points.size() == 4) {
        // Return size of the 2D quadriliteral
        // -> Gaussian shoelace formula
        return 0.5f * ( abs( (points[0].y-points[2].y)*(points[3].x-points[1].x) + (points[1].y-points[3].y)*(points[0].x-points[2].x) ) );
    } else {
        return 0;
    }
}

cv::Mat Marker::rotate(cv::Mat in)
{
  cv::Mat out;
  in.copyTo(out);
  for (int i=0;i<in.rows;i++)
  {
    for (int j=0;j<in.cols;j++)
    {
      out.at<uchar>(i,j)=in.at<uchar>(in.cols-j-1,i);
    }
  }
  return out;
}

int Marker::hammDistMarker(cv::Mat bits)
{
  int ids[4][5]=
  {
    {1,0,0,0,0},
    {1,0,1,1,1},
    {0,1,0,0,1},
    {0,1,1,1,0}
  };
  
  int dist=0;
  
  for (int y=0;y<5;y++)
  {
    int minSum=1e5; //hamming distance to each possible word
    
    for (int p=0;p<4;p++)
    {
      int sum=0;
      //now, count
      for (int x=0;x<5;x++)
      {
        sum += bits.at<uchar>(y,x) == ids[p][x] ? 0 : 1;
      }
      
      if (minSum>sum)
        minSum=sum;
    }
    
    //do the and
    dist += minSum;
  }
  
  return dist;
}

int Marker::mat2id(const cv::Mat &bits)
{
  int val=0;
  for (int y=0;y<5;y++)
  {
    val<<=1;
    if ( bits.at<uchar>(y,1)) val|=1;
    val<<=1;
    if ( bits.at<uchar>(y,3)) val|=1;
  }
  return val;
}

cv::Mat Marker::getMarkerBitCode(cv::Mat &markerImage,int &nRotations, unsigned int regions) {
    assert(markerImage.rows == markerImage.cols);
    assert(markerImage.type() == CV_8UC1);

    cv::Mat grey = markerImage;

    // Threshold image
    cv::threshold(grey, grey, 125, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

  #ifdef SHOW_DEBUG_IMAGES
    cv::imshow( "Marker ID Extraction", markerImage );
    //cv::showAndSave("Binary marker", grey);
    InstantIOLogger::addLog("Trying to save " + ToString(grey.rows) 
              + "x" + ToString(grey.cols) + " marker image.");
      if (cv::imwrite("d:/Marker_binary.png", grey)) {
          InstantIOLogger::addLog("Saved Marker_binary.png");
      }
  #endif

    //Markers  are divided in regions, of which the inner belongs to marker info
    //the external border should be entirely black

    int cellSize = markerImage.rows / regions;

    for (int y=0;y<regions;y++)
    {
      int inc = regions - 1;

      if (y==0 || y==regions) inc=1; //for first and last row, check the whole border

      for (int x=0;x<regions;x+=inc)
      {
        int cellX = x * cellSize;
        int cellY = y * cellSize;
        cv::Mat cell = grey(cv::Rect(cellX,cellY,cellSize,cellSize));

        int nZ = cv::countNonZero(cell);

        if (nZ > (cellSize*cellSize) / 2)
        {
          return cv::Mat();//can not be a marker because the border element is not black!
        }
      }
    }

    cv::Mat bitMatrix = cv::Mat::zeros(regions-2,regions-2,CV_8UC1);

    //get information(for each inner square, determine if it is  black or white)  
    for (int y=0;y<regions-2;y++)
    {
      for (int x=0;x<regions-2;x++)
      {
        int cellX = (x+1)*cellSize;
        int cellY = (y+1)*cellSize;
        cv::Mat cell = grey(cv::Rect(cellX,cellY,cellSize,cellSize));

        int nZ = cv::countNonZero(cell);
        if (nZ> (cellSize*cellSize) /2) 
          bitMatrix.at<uchar>(y,x) = 1;
      }
    }

    //check all possible rotations
    cv::Mat rotations[4];
    int distances[4];

    rotations[0] = bitMatrix;  
    distances[0] = hammDistMarker(rotations[0]);

    std::pair<int,int> minDist(distances[0],0);

    for (int i=1; i<4; i++)
    {
      //get the hamming distance to the nearest possible word
      rotations[i] = rotate(rotations[i-1]);
      distances[i] = hammDistMarker(rotations[i]);

      if (distances[i] < minDist.first)
      {
        minDist.first  = distances[i];
        minDist.second = i;
      }
    }

    nRotations = minDist.second;
    if (minDist.first == 0)
    {
      cv::Mat bitCode = rotations[minDist.second];
      return bitCode;
    }

    return cv::Mat();
    
}


int Marker::getMarkerId(cv::Mat &bits)
{
    return mat2id(bits);
}

cv::Point2f Marker::get_center(){
    cv::Point2f result(0,0);
    std::vector<cv::Point2f>::iterator it;
    for(it = points.begin(); it < points.end(); it++){
        result += *it;
    }
    return result * (1.0/points.size());
}

void Marker::drawCross(cv::Mat& image, cv::Scalar color, std::string text){
    float thickness = 2;
    cv::line(image, points[0], points[2], color, thickness, cv::LINE_AA);
    cv::line(image, points[3], points[1], color, thickness, cv::LINE_AA);
    drawText(image, get_center()+cv::Point2f(0,10), (image.channels()==1)?cv::Scalar(200):cv::Scalar(0,0,255), text);
}

void Marker::drawContour(cv::Mat& image, cv::Scalar color, std::string text){
    float thickness = 2;

    cv::Scalar mflow_color;

    if( tracked_points.size() > 0 ) {
        mflow_color = cv::Scalar( 0, 0, 255 );
        cv::line(image, tracked_points[0], tracked_points[1], mflow_color, 4, cv::LINE_AA);
        cv::line(image, tracked_points[1], tracked_points[2], mflow_color, 4, cv::LINE_AA);
        cv::line(image, tracked_points[2], tracked_points[3], mflow_color, 4, cv::LINE_AA);
        cv::line(image, tracked_points[3], tracked_points[0], mflow_color, 4, cv::LINE_AA);
    }

    if( tracked )
        color = cv::Scalar( 0, 0, 255 );
    //else
    //    color = cv::Scalar( 0, 255, 0 );

    cv::line(image, points[0], points[1], color, thickness, cv::LINE_AA);
    cv::line(image, points[1], points[2], color, thickness, cv::LINE_AA);
    cv::line(image, points[2], points[3], color, thickness, cv::LINE_AA);
    cv::line(image, points[3], points[0], color, thickness, cv::LINE_AA);

	//for (unsigned int p = 0; p < calculatedGoodFeaturesToTrack.size(); ++p) {
	//	cv::circle(image, calculatedGoodFeaturesToTrack[p], 2, color, thickness, cv::LINE_AA);
	//}

    drawText(image, get_center(), (image.channels()==1)?cv::Scalar(200):cv::Scalar(0,0,255), text);
}

void Marker::drawText(cv::Mat& image, cv::Point2f position, cv::Scalar color, std::string text){
    if (text == ""){
        //default to id:
		std::stringstream s;
		s << "["  << id << "]";
        cv::putText(image, s.str(), position, cv::FONT_HERSHEY_SIMPLEX, 1.0, color, 1, cv::LINE_AA);
    }else{
        //string passed as param:
        cv::putText(image, text, position, cv::FONT_HERSHEY_PLAIN, 1.0, color, 1, cv::LINE_AA);
    }
}


std::vector< cv::Point2f > 
Marker
::getGoodFeaturesToTrack( cv::Mat& img) {
	std::vector< cv::Point2f > corners;
	//cvNamedWindow("MedianFlow");
	cv::Rect previousBoundingBox = cv::boundingRect(points);
	// Add some "safety zone"
	previousBoundingBox.x -= 2;
	previousBoundingBox.y -= 2;
	previousBoundingBox.width += 4;
	previousBoundingBox.height += 4;
	if( previousBoundingBox.x < 0 ) { previousBoundingBox.width += previousBoundingBox.x; previousBoundingBox.x = 0; }
	if( previousBoundingBox.y < 0 ) { previousBoundingBox.height += previousBoundingBox.y; previousBoundingBox.y = 0; }
	if( previousBoundingBox.x > img.cols ) return corners;
	if( previousBoundingBox.y > img.rows ) return corners;
	if( previousBoundingBox.x + previousBoundingBox.width > img.cols ) { previousBoundingBox.width = img.cols - previousBoundingBox.x; }
	if( previousBoundingBox.y + previousBoundingBox.height > img.rows ) { previousBoundingBox.height = img.rows - previousBoundingBox.y; }
	if( previousBoundingBox.width <= 0 ) return corners;
	if( previousBoundingBox.height <= 0 ) return corners;

	rectangle( img, cv::Point( previousBoundingBox.x, previousBoundingBox.y ), cv::Point( previousBoundingBox.x + previousBoundingBox.width, previousBoundingBox.y + previousBoundingBox.height ), cv::Scalar(0,255,0), 1, 8 );

	cv::Mat region( img, previousBoundingBox );
	cv::goodFeaturesToTrack( region, corners, 30, 0.1, 5 );//, mask );//, mask, 3, false, 0.04 );
	for( int g = 0; g < corners.size(); ++g ) {
		corners[g].x = corners[g].x + previousBoundingBox.x;
		corners[g].y = corners[g].y + previousBoundingBox.y;
	}
	corners.insert( corners.begin(), points.begin(), points.end() );

	return corners;
}


void
Marker::calculateGoodFeaturesToTrack() {
    std::vector<cv::Point2f> corners;
    std::vector< cv::Point2f > cornersRectified;
    std::vector< cv::Point3f > rectifiedPoints;
    
    // Precompute bit matrix with outer black bits
    cv::Mat extBitMat = cv::Mat::zeros(bitCode.rows+2, bitCode.cols+2, CV_8U);
    cv::Mat tmp = extBitMat(cv::Rect(1,1,bitCode.rows,bitCode.cols));
    bitCode.copyTo(tmp);
    
    // Precompute inverse transformation matrix
    cv::Mat inverseTransformation = cv::getPerspectiveTransform(m_markerCorners2d, &points[0]);
    
    // Insert inner interesting points
    for (unsigned int i = 1; i < extBitMat.rows; ++i) {
        for (unsigned int j = 1; j < extBitMat.cols; ++j) {
            // look for points in the upper left corners of each bit rectangle
            //  x__ _
            //  |  | ...
            bool current = extBitMat.at<bool>(i,j);
            if ( !(  current == extBitMat.at<bool>(i  ,j-1) 
                  && current == extBitMat.at<bool>(i-1,j-1)
                  && current == extBitMat.at<bool>(i-1,j  )
                 ||  current == extBitMat.at<bool>(i  ,j-1) 
                  && current != extBitMat.at<bool>(i-1,j-1)
                  && current != extBitMat.at<bool>(i-1,j)
                 ||  current != extBitMat.at<bool>(i  ,j-1) 
                  && current != extBitMat.at<bool>(i-1,j-1)
                  && current == extBitMat.at<bool>(i-1,j  ) )) {
                
                cv::Point2f cornerPos2d( ((float)j / (float)extBitMat.cols) * 100.0f, 
                                       ((float)i / (float)extBitMat.rows) * 100.0f );
                
                cornersRectified.push_back(cornerPos2d);
            }
        }
    }
    
    cv::perspectiveTransform(cornersRectified, corners, inverseTransformation);
    
    // Insert outer corners
    corners.insert( corners.begin(), points.begin(), points.end() );
    cornersRectified.insert( cornersRectified.begin(), m_markerCorners2d, m_markerCorners2d + 4 );
    
    // Adapt rectified points to marker size
    cv::Point3f p;
    for (unsigned int i = 0; i < cornersRectified.size(); ++i) {
        p.x = cornersRectified[i].x / 100 * this->size.width - (this->size.width / 2);
        p.y = cornersRectified[i].y / 100 * this->size.height - (this->size.height / 2);
        p.z = 0;
        rectifiedPoints.push_back(p);
    }
    
    this->calculatedGoodFeaturesToTrack = corners;
    this->rectifiedGoodFeaturesToTrack = rectifiedPoints;
    
}


void Marker::translate(cv::Point2f dist){
    for(int i=0; i<points.size(); i++){
        points[i] = points[i] + dist;
    }
}
