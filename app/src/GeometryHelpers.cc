#define _USE_MATH_DEFINES
#include <math.h>

#include "GeometryHelpers.h"
#include <stdio.h>
#include <opencv2/core/core.hpp>



cv::Vec4f 
GeometryHelpers::rotationMat2Quaternion(cv::Mat &rotationMat) {
    cv::Vec4f q;
    
    float trace = rotationMat.at<float>(0,0) + rotationMat.at<float>(1,1) + rotationMat.at<float>(2,2) + 1.0f;

    if( trace > 0.001 ) {
        float S = 0.5f / sqrt(trace);
        q[0] = ( rotationMat.at<float>(2,1) - rotationMat.at<float>(1,2) ) * S;
        q[1] = ( rotationMat.at<float>(0,2) - rotationMat.at<float>(2,0) ) * S;
        q[2] = ( rotationMat.at<float>(1,0) - rotationMat.at<float>(0,1) ) * S;
        q[3] = 0.25f / S;
    }
    
    else if (rotationMat.at<float>(0,0) > rotationMat.at<float>(1,1) && rotationMat.at<float>(0,0) > rotationMat.at<float>(2,2)) {
        float S = sqrt( 1.0f + rotationMat.at<float>(0,0) - rotationMat.at<float>(1,1) - rotationMat.at<float>(2,2) ) * 2.0f;
        q[0] = 0.25f * S; 
        q[1] = (rotationMat.at<float>(0,1) + rotationMat.at<float>(1,0) ) / S;
        q[2] = (rotationMat.at<float>(0,2) + rotationMat.at<float>(2,0) ) / S;
        q[3] = (rotationMat.at<float>(1,2) - rotationMat.at<float>(2,1) ) / S; 

    } else if (rotationMat.at<float>(1,1) > rotationMat.at<float>(2,2)) {
        float S = sqrt( 1.0f + rotationMat.at<float>(1,1) - rotationMat.at<float>(0,0) - rotationMat.at<float>(2,2) ) * 2.0f;
        q[0] = (rotationMat.at<float>(0,1) + rotationMat.at<float>(1,0) ) / S;
        q[1] = 0.25f * S; 
        q[2] = (rotationMat.at<float>(1,2) + rotationMat.at<float>(2,1) ) / S;
        q[3] = (rotationMat.at<float>(0,2) - rotationMat.at<float>(2,0) ) / S; 

    } else {
        float S = sqrt( 1.0f + rotationMat.at<float>(2,2) - rotationMat.at<float>(0,0) - rotationMat.at<float>(1,1) ) * 2.0f;
        q[0] = (rotationMat.at<float>(0,2) + rotationMat.at<float>(2,0) ) / S;
        q[1] = (rotationMat.at<float>(1,2) + rotationMat.at<float>(2,1) ) / S;
        q[2] = 0.25f * S; 
        q[3] = (rotationMat.at<float>(0,1) - rotationMat.at<float>(1,0) ) / S; 
    }
    
    
    float r = cv::norm(q);
    q[0] /= r;
    q[1] /= r;
    q[2] /= r;
    q[3] /= r;
    
    return q;
}

cv::Vec3f 
GeometryHelpers::rotationMat2EulerAngles(cv::Mat& rotationMat) {
    cv::Vec3f euler;
    euler[0] = atan2(rotationMat.at<float>(2,1), rotationMat.at<float>(2,2));
    euler[1] = atan2(-1.0f * rotationMat.at<float>(2,0), 
                     sqrt(rotationMat.at<float>(2,1)*rotationMat.at<float>(2,1) + rotationMat.at<float>(2,2)*rotationMat.at<float>(2,2)));
    euler[2] = atan2(rotationMat.at<float>(1,0), rotationMat.at<float>(0,0));
    
    return euler;
}



cv::Vec4f 
GeometryHelpers::slerpQuaternions(cv::Vec4f q1, cv::Vec4f q2, float t) {
    cv::Vec4f res;
    float dot = q1.dot(q2);

    // dot = cos(theta)
    // if (dot < 0), q1 and q2 are more than 90 degrees apart,
    // so we can invert one to reduce spinning	
    if (dot < 0) {
        dot = -dot;
        res = -q2;
    } else {
        res = q2;
    }

    if (dot < 0.95f) {
        float angle = acos(dot);
        return (q1*sin(angle*(1-t)) + res*sin(angle*t)) * (1.0f / sin(angle));
    } else { // if the angle is small, use linear interpolation								
        res = q1*(1-t) + q2*t;
        return res * (1.0f / cv::norm(res));
    }
}

cv::Vec4f 
GeometryHelpers::quaternion2AxisAngle(cv::Vec4f q) {
    // Normalize quaternion
    q *= 1.0f / cv::norm(q);
    
    cv::Vec4f res;
    
    res[3] = 2 * acos(q[3]);
    res[0] = q[0] / sqrt(1-q[3]*q[3]);
    res[1] = q[1] / sqrt(1-q[3]*q[3]);
    res[2] = q[2] / sqrt(1-q[3]*q[3]);
    
    return res;
}

/*
 * reproject a body/marker position from 3d back to 2d image coordinates
 *
 * @param m
 * @param coord_2d
 * @return distance in m
 */
float GeometryHelpers::reprojectTo2D(const cv::Mat camera_matrix, const cv::Mat& m, cv::Point2f &coord_2d){
    //decompose to translation
    cv::Mat translation(3,1,CV_64F);
    assert(m.type() == CV_32F);

    translation.at<double>(0) = -(double)m.at<float>(3,0);
    translation.at<double>(1) =  (double)m.at<float>(3,1);
    translation.at<double>(2) =  (double)m.at<float>(3,2);

    cv::Mat output = camera_matrix * translation;

    coord_2d.x = output.at<double>(0) / output.at<double>(2);
    coord_2d.y = output.at<double>(1) / output.at<double>(2);

    return m.at<float>(3,2);
}

/*
 * reproject a body/marker position from 3d back to 2d image coordinates
 *
 * @param m
 * @param coord_2d
 * @return distance in m
 */
float GeometryHelpers::reprojectTo2D(const cv::Mat camera_matrix, const cv::Point3d& p, cv::Point2f &coord_2d){
    //decompose to translation
    cv::Mat translation(3,1,CV_64F);
    translation.at<double>(0) = -p.x;
    translation.at<double>(1) = p.y;
    translation.at<double>(2) = p.z;

    cv::Mat output = camera_matrix * translation;

    coord_2d.x = output.at<double>(0) / output.at<double>(2);
    coord_2d.y = output.at<double>(1) / output.at<double>(2);

    return p.z;
}

/*
 * reproject a body/marker position from 3d back to 2d image coordinates using the OpenCV projectPoints method.
 *
 */
void GeometryHelpers::reprojectToDistorted2D(const cv::Mat camera_matrix, 
                                     const cv::Mat distortion_coefficients, 
                                     std::vector<cv::Point3f>& points_3d, 
                                     std::vector<cv::Point2f>& coord_2d) {

    std::vector<cv::Point3f> transformedPoints;
    std::vector<cv::Point3f>::iterator pIt;
    for (pIt = points_3d.begin(); pIt != points_3d.end(); ++pIt) {
        cv::Point3f p = *pIt;
        p.x *= -1.0f; // Convert coordinate system to opencv internal
        p *= 1000.0f; // transfrom meter coordinates to millimeter
        transformedPoints.push_back(p);
    }

    // Project to 2D image coordinates
    cv::Mat rVec = cv::Mat::zeros(3,1,CV_32F);
    cv::Mat tVec = cv::Mat::zeros(3,1,CV_32F);
    cv::projectPoints(transformedPoints, rVec, tVec, camera_matrix, distortion_coefficients, coord_2d);
    
}


//this should be moved to a new body class?!
//fixme: change axis colors to match standard
void GeometryHelpers::drawCoordinateSystem(cv::Mat image, const cv::Mat camera_matrix, const cv::Mat& m){
    cv::Point2f center_of_gravity_px;
    reprojectTo2D(camera_matrix, m, center_of_gravity_px);

    assert(m.type() == CV_32F);
    cv::Mat rotation = m(cv::Rect(0,0,3,3));
    cv::Mat translation = m(cv::Rect(0,3,3,1)).t();

    for(int i=0; i<3; i++){
        cv::Mat axis = (cv::Mat_<float>(3, 1) << ((i==0)?-1:0),((i==2)?-1:0),((i==1)?-1:0));
        cv::Mat result = (rotation.inv() * axis);
        cv::normalize(result,result);
        result = result * 0.05 + translation;

        cv::Mat result_double(3,1,CV_64F)  ;
        result_double.at<double>(0) = -(double)result.at<float>(0,0);
        result_double.at<double>(1) =  (double)result.at<float>(0,1);
        result_double.at<double>(2) =  (double)result.at<float>(0,2);

        cv::Mat output = camera_matrix * result_double;


        cv::Point2f target(output.at<double>(0)/output.at<double>(2), output.at<double>(1)/output.at<double>(2));
        cv::Scalar color;
        switch(i){
            case(0): color=cv::Scalar(0,0,155); break;
            case(1): color=cv::Scalar(0,155,0); break;
            case(2): color=cv::Scalar(155,0,0); break;
        }
        cv::line(image, center_of_gravity_px, target, color,2,cv::LINE_AA);
    }
}


//this should be moved to a new body class?!
//fixme: change axis colors to match standard
void GeometryHelpers::draw3DLines2D(cv::Mat image, const cv::Mat camera_matrix, const cv::Mat distortion_matrix, const cv::Mat& m, std::vector<cv::Point3d> lines, cv::Scalar color, int width){
    std::vector<cv::Point3f> lines_3d;
	for(unsigned int i = 0; i < lines.size(); ++i){
        lines_3d.push_back(lines[i]);
    }
    std::vector<cv::Point2f> lines_2d;
    reprojectToDistorted2D(camera_matrix, distortion_matrix, lines_3d, lines_2d);

    cv::Point2d pt0_2d;
    cv::Point2d pt1_2d;

    //draw lines:
    for(std::vector<cv::Point2f>::iterator it = lines_2d.begin(); ((it) != lines_2d.end()) && ((it+1) != lines_2d.end()); ){
        pt0_2d = *it++;
        pt1_2d = *it++;
        cv::line(image, pt0_2d, pt1_2d, color, width, cv::LINE_AA);
    }
}

//distort a single point. this is uesful if you have a debug/preview image with
//the distorted live view from the camera and you do backprojections from3d to 2d.
//if you do not distort the points they end up at the wrong image position
//see http://code.opencv.org/issues/1387
//somehow broken/does not work correctly
cv::Point2f GeometryHelpers::distortPoint(cv::Point2f p, const cv::Mat & camera_matrix, const cv::Mat & distorsion_matrix){
    double x = p.x;
    double y = p.y;

    assert(camera_matrix.type() == CV_64F);
    double fx = camera_matrix.at<double>(0,0);
    double fy = camera_matrix.at<double>(1,1);
    double ux = camera_matrix.at<double>(0,2);
    double uy = camera_matrix.at<double>(1,2);

    assert(distorsion_matrix.type() == CV_64F);
    double k1 = distorsion_matrix.at<double>(0);
    double k2 = distorsion_matrix.at<double>(1);
    double p1 = distorsion_matrix.at<double>(2);
    double p2 = distorsion_matrix.at<double>(3);
    double k3 = distorsion_matrix.at<double>(4);

    double xCorrected, yCorrected;
    //Step 1 : correct distorsion
    {
      double r2 = x*x + y*y;
      //radial distorsion
      xCorrected = x * (1. + k1 * r2 + k2 * r2 * r2 + k3 * r2 * r2 * r2);
      yCorrected = y * (1. + k1 * r2 + k2 * r2 * r2 + k3 * r2 * r2 * r2);

      //tangential distorsion
      //The "Learning OpenCV" book is wrong here !!!
      //False equations from the "Learning OpenCv" book below :
      //xCorrected = xCorrected + (2. * p1 * y + p2 * (r2 + 2. * x * x));
      //yCorrected = yCorrected + (p1 * (r2 + 2. * y * y) + 2. * p2 * x);
      //Correct formulae found at : http://www.vision.caltech.edu/bouguetj/calib_doc/htmls/parameters.html
      xCorrected = xCorrected + (2. * p1 * x * y + p2 * (r2 + 2. * x * x));
      yCorrected = yCorrected + (p1 * (r2 + 2. * y * y) + 2. * p2 * x * y);
    }
    //Step 2 : ideal coordinates => actual coordinates
    {
      xCorrected = xCorrected * fx + ux;
      yCorrected = yCorrected * fy + uy;
    }

    printf("FIXME: THIS IS SOMEHOW BROKEN!\n");
    printf("DISTORT PT  %4.1f %4.1f ---> %4.1f %4.1f\n",x,y,xCorrected,yCorrected);
    fflush(stdout); exit(0);
    return cv::Point2d(xCorrected, yCorrected);
}

/*
 * reproject a body/marker position from 3d back to 2d image coordinates
 *
 * @param m
 * @param coord_2d
 * @return distance in m
 */
float GeometryHelpers::reprojectTo2D(const cv::Mat camera_matrix, cv::Vec3f point, cv::Point2f &coord_2d){
    //decompose to translation
    cv::Mat translation(3,1,CV_64F);
    translation.at<double>(0) = -point[0];
    translation.at<double>(1) =  point[1];
    translation.at<double>(2) =  point[2];

    cv::Mat output = camera_matrix * translation;

    coord_2d.x = output.at<double>(0) / output.at<double>(2);
    coord_2d.y = output.at<double>(1) / output.at<double>(2);

    return point[2];
}

cv::Mat 
GeometryHelpers::constructMat(cv::Vec3f translation, cv::Vec4f orientation) {
    cv::Mat res = cv::Mat::zeros(4, 4, CV_32F);

    if (orientation[0] == 0 && orientation[1] == 0 && orientation[2] == 0
            || orientation[3] == 0) {
        res.at<float>(0,0) = 1;
        res.at<float>(1,1) = 1;
        res.at<float>(2,2) = 1;
    } else {
        double c = cos(orientation[3]);
        double s = sin(orientation[3]);
        double t = 1.0 - c;
        // normalize
        double magnitude = sqrt(orientation[0]*orientation[0] + orientation[1]*orientation[1] + orientation[2]*orientation[2]);
        orientation[0] /= magnitude;
        orientation[1] /= magnitude;
        orientation[2] /= magnitude;

        res.at<float>(0,0) = c + orientation[0]*orientation[0]*t;
        res.at<float>(1,1) = c + orientation[1]*orientation[1]*t;
        res.at<float>(2,2) = c + orientation[2]*orientation[2]*t;
        double tmp1 = orientation[0]*orientation[1]*t;
        double tmp2 = orientation[2]*s;
        res.at<float>(1,0) = tmp1 + tmp2;
        res.at<float>(0,1) = tmp1 - tmp2;
        tmp1 = orientation[0]*orientation[2]*t;
        tmp2 = orientation[1]*s;
        res.at<float>(2,0) = tmp1 - tmp2;
        res.at<float>(0,2) = tmp1 + tmp2;    
        tmp1 = orientation[1]*orientation[2]*t;
        tmp2 = orientation[0]*s;
        res.at<float>(2,1) = tmp1 + tmp2;
        res.at<float>(1,2) = tmp1 - tmp2;
    }
    
    // translation
    res.at<float>(3,0) = translation[0];
    res.at<float>(3,1) = translation[1];
    res.at<float>(3,2) = translation[2];
    res.at<float>(3,3) = 1;
    
    return res;
}

void 
GeometryHelpers::decomposeMat(const cv::Mat& m, cv::Vec3f& translation, cv::Vec4f& orientation) {
    decomposeMat(m, translation);
    decomposeMat(m, orientation);
}

void
GeometryHelpers::decomposeMat(const cv::Mat& m, cv::Mat& translation, cv::Mat& rotation){
    rotation = m(cv::Rect(0,0,3,3));
    translation = m(cv::Rect(0,3,3,1)).t();
}

void 
GeometryHelpers::decomposeMat(const cv::Mat& m, cv::Vec3f& translation) {
    translation[0] = m.at<float>(3,0);
    translation[1] = m.at<float>(3,1);
    translation[2] = m.at<float>(3,2);
}




void GeometryHelpers::decomposeMat(const cv::Mat& m, cv::Vec4f& orientation) {
    float angle, x, y, z; // variables for result
    float epsilon = 0.01; // margin to allow for rounding errors
    float epsilon2 = 0.1; // margin to distinguish between 0 and 180 degrees
    // optional check that input is pure rotation, 'isRotationMatrix' is defined at:
    // http://www.euclideanspace.com/maths/algebra/matrix/orthogonal/rotation/
    if ((fabs(m.at<float>(0,1) - m.at<float>(1,0)) < epsilon)
            && (fabs(m.at<float>(0,2) - m.at<float>(2,0)) < epsilon)
            && (fabs(m.at<float>(1,2) - m.at<float>(2,1)) < epsilon)) {
        // singularity found
        // first check for identity matrix which must have +1 for all terms
        //  in leading diagonaland zero in other terms
        if ((fabs(m.at<float>(0,1) + m.at<float>(1,0)) < epsilon2)
                && (fabs(m.at<float>(0,2) + m.at<float>(2,0)) < epsilon2)
                && (fabs(m.at<float>(1,2) + m.at<float>(2,1)) < epsilon2)
                && (fabs(m.at<float>(0,0) + m.at<float>(1,1) + m.at<float>(2,2) - 3.0f) < epsilon2)) {
            // this singularity is identity matrix so angle = 0
            orientation = cv::Vec4f(0,1,0,0); // zero angle, arbitrary axis
            return;
        }
        // otherwise this singularity is angle = 180
        angle = M_PI;
        double xx = (m.at<float>(0,0) + 1.0f) / 2.0f;
        double yy = (m.at<float>(1,1) + 1.0f) / 2.0f;
        double zz = (m.at<float>(2,2) + 1.0f) / 2.0f;
        double xy = (m.at<float>(0,1) + m.at<float>(1,0)) / 4.0f;
        double xz = (m.at<float>(0,2) + m.at<float>(2,0)) / 4.0f;
        double yz = (m.at<float>(1,2) + m.at<float>(2,1)) / 4.0f;
        if ((xx > yy) && (xx > zz)) { // m.at<float>(0,0) is the largest diagonal term
            if (xx < epsilon) {
                x = 0;
                y = 0.7071;
                z = 0.7071;
            } else {
                x = sqrt(xx);
                y = xy / x;
                z = xz / x;
            }
        } else if (yy > zz) { // m.at<float>(1,1) is the largest diagonal term
            if (yy < epsilon) {
                x = 0.7071;
                y = 0;
                z = 0.7071;
            } else {
                y = sqrt(yy);
                x = xy / y;
                z = yz / y;
            }
        } else { // m.at<float>(1,1) is the largest diagonal term so base result on this
            if (zz < epsilon) {
                x = 0.7071;
                y = 0.7071;
                z = 0;
            } else {
                z = sqrt(zz);
                x = xz / z;
                y = yz / z;
            }
        }
        orientation = cv::Vec4f(x, y, z, angle);
        return;
    }
    // as we have reached here there are no singularities so we can handle normally
    double s = sqrt((m.at<float>(2,1) - m.at<float>(1,2))*(m.at<float>(2,1) - m.at<float>(1,2))
            +(m.at<float>(0,2) - m.at<float>(2,0))*(m.at<float>(0,2) - m.at<float>(2,0))
            +(m.at<float>(1,0) - m.at<float>(0,1))*(m.at<float>(1,0) - m.at<float>(0,1))); // used to normalise
    if (fabs(s) < 0.001) s = 1.0f;
    // prevent divide by zero, should not happen if matrix is orthogonal and should be
    // caught by singularity test above, but I've left it in just in case
    angle = acos((m.at<float>(0,0) + m.at<float>(1,1) + m.at<float>(2,2) - 1.0f) / 2.0f);
    x = (m.at<float>(2,1) - m.at<float>(1,2)) / s;
    y = (m.at<float>(0,2) - m.at<float>(2,0)) / s;
    z = (m.at<float>(1,0) - m.at<float>(0,1)) / s;
    orientation = cv::Vec4f(x, y, z, angle);
}


cv::Mat 
GeometryHelpers::invertMat(cv::Mat input) {
    cv::Mat out = input.clone();
    
    cv::Mat rotROI(out, cv::Rect(0,0,3,3));
    rotROI = rotROI.t();
    cv::Mat transROI(out, cv::Rect(0,3,3,1));
    transROI = transROI * -1.0f;
    
    return out;
}

cv::Mat GeometryHelpers::interpolate(cv::Mat m1, cv::Mat m2, float factor) {
    cv::Vec3f translation1, translation2, interpolatedTrans;
    cv::Mat rotMat1, rotMat2;
    cv::Vec4f quaternion1, quaternion2;
    cv::Vec4f interpolatedRotQuaternion;
    
    decomposeMat(m1, translation1);
    decomposeMat(m2, translation2);
    
    cv::Rect rotRect(0,0,3,3);
    rotMat1 = m1(rotRect);
    rotMat2 = m2(rotRect);
    quaternion1 = rotationMat2Quaternion(rotMat1);
    quaternion2 = rotationMat2Quaternion(rotMat2);
    
    interpolatedTrans[0] = (translation1[0] + translation2[0]*factor) / (1.0f + factor);
    interpolatedTrans[1] = (translation1[1] + translation2[1]*factor) / (1.0f + factor);
    interpolatedTrans[2] = (translation1[2] + translation2[2]*factor) / (1.0f + factor);
    
    interpolatedRotQuaternion = slerpQuaternions(quaternion1, quaternion2, factor);
    
    return constructMat(interpolatedTrans, quaternion2AxisAngle(interpolatedRotQuaternion));
}


cv::Point3f GeometryHelpers::multPointMatrix(const cv::Point3f &src, const cv::Mat &m) 
{
    cv::Point3f dst;
    dst.x = src.x * m.at<float>(0,0) + src.y * m.at<float>(1,0) + src.z * m.at<float>(2,0) + m.at<float>(3,0);
    dst.y = src.x * m.at<float>(0,1) + src.y * m.at<float>(1,1) + src.z * m.at<float>(2,1) + m.at<float>(3,1);
    dst.z = src.x * m.at<float>(0,2) + src.y * m.at<float>(1,2) + src.z * m.at<float>(2,2) + m.at<float>(3,2);
    float w = src.x * m.at<float>(0,3) + src.y * m.at<float>(1,3) + src.z * m.at<float>(2,3) + m.at<float>(3,3);
    if (w != 1 && w != 0) {
        dst.x /= w;
        dst.y /= w;
        dst.z /= w;
    }
    return dst;
}



