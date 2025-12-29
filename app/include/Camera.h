/*
 * File:   Camera.h
 * Author: prenner
 *
 * Created on July 1, 2014, 3:32 PM
 */

#ifndef CAMERA_H
#define	CAMERA_H

#include <string>
#include <opencv2/opencv.hpp>


/**
 * Camera struct for handling multiple cameras.
 */
struct Camera {

    /**
     * Constructor.
     */
    Camera();
    /**
     * Constructor.
     * @param name name of the camera
     * @param calibrationUrl calibrationUrl (leave empty for no calibration)
     */
    Camera(std::string name, std::string calibrationUrl, bool isStatic);
    /**
     * Constructor.
     * @param name name of the camera
     * @param camMatrix
     * @param distCoeff
     */
    Camera(std::string name, cv::Mat camMatrix, cv::Mat distCoeff, bool isStatic);



    void setCamMatrix(std::vector<double> data, int rows, int cols);
    void setDistCoeff(std::vector<double> data, int rows, int cols);

    virtual ~Camera();

    std::string name;
    std::string calibrationUrl;
    bool isStatic;

    cv::Mat camMatrix;
    cv::Mat distCoeff;
    cv::Size resolution;

    /**
     * Reads the camera parameters from file.
     * @param url
     * @return success
     */
    bool readCalibration(std::string url);
    bool readCalibrationXML();
    bool readCalibrationYAML();


};

#endif	/* CAMERA_H */

