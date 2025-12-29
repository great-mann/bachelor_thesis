/* 
 * File:   GeometryHelpers.h
 * Author: prenner
 *
 * Created on May 26, 2014, 3:12 PM
 */

#ifndef GEOMETRYHELPERS_H
#define	GEOMETRYHELPERS_H

#include <opencv2/opencv.hpp>


class GeometryHelpers {
    
public:

    static cv::Point2f distortPoint(cv::Point2f p, const cv::Mat & camera_matrix, const cv::Mat & distorsion_matrix);
    /**
     * Constructs a 4x4 matrix from translation and orientation.
     * @param translation translation vector
     * @param orientation orientation quaternion
     * @return 4x4 matrix
     */
    static cv::Mat constructMat(cv::Vec3f translation, cv::Vec4f orientation);
    static void decomposeMat(const cv::Mat &m, cv::Vec3f &translation);
    static void decomposeMat(const cv::Mat &m, cv::Vec4f &orientation);
    static void decomposeMat(const cv::Mat &m, cv::Vec3f &translation, cv::Vec4f &orientation);
    static void decomposeMat(const cv::Mat& m, cv::Mat& translation, cv::Mat& rotation);

    static float reprojectTo2D(const cv::Mat camera_matrix, const cv::Mat& m, cv::Point2f &coord_2d);
    static float reprojectTo2D(const cv::Mat camera_matrix, cv::Vec3f point, cv::Point2f &coord_2d);
    static float reprojectTo2D(const cv::Mat camera_matrix, const cv::Point3d& p, cv::Point2f &coord_2d);
    static void reprojectToDistorted2D(const cv::Mat camera_matrix, 
                                         const cv::Mat distortion_coefficients, 
                                         std::vector<cv::Point3f> &points_3d, 
                                         std::vector<cv::Point2f> &coord_2d);
    static void drawCoordinateSystem(cv::Mat image, const cv::Mat camera_matrix, const cv::Mat& m);
    static void draw3DLines2D(cv::Mat image, const cv::Mat camera_matrix, const cv::Mat distortion_matrix, const cv::Mat& m, std::vector<cv::Point3d> lines, cv::Scalar color, int width=2);

    static cv::Mat invertMat(cv::Mat input);
    
    static cv::Mat interpolate(cv::Mat m1, cv::Mat m2, float factor);
    
    static cv::Point3f multPointMatrix(const cv::Point3f &src, const cv::Mat &m);
    
    static cv::Vec4f rotationMat2Quaternion(cv::Mat &rotationMat);
    static cv::Vec3f rotationMat2EulerAngles(cv::Mat &rotationMat);
    static cv::Vec4f slerpQuaternions(cv::Vec4f q1, cv::Vec4f q2, float t);
    static cv::Vec4f quaternion2AxisAngle(cv::Vec4f q);
 
private:
    static inline float signum(float x) {return (x >= 0.0f) ? +1.0f : -1.0f;};
    
};

#endif	/* GEOMETRYHELPERS_H */

