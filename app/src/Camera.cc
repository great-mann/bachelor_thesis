#include "Camera.h"
#ifdef YAML_CPP_AVAILABLE
#include <yaml-cpp/yaml.h>
#endif

Camera::Camera() {
    this->name = "default";
    this->calibrationUrl = "";
    this->isStatic = false;
}


Camera::Camera(std::string name, std::string calibrationUrl, bool isStatic) {
    this->name = name;
    this->calibrationUrl = calibrationUrl;
    this->isStatic = isStatic;

    readCalibration(calibrationUrl);
}



Camera::~Camera() {}

bool
Camera::readCalibration(std::string url) {
    this->calibrationUrl = url;

    if (url == "none"){
        //do not try to load the config, use none
        return true;
    }

    if(url.substr(url.find_last_of(".") + 1) == "yaml") {
        return readCalibrationYAML();
    }else{
        return readCalibrationXML();
    }
}

bool
Camera::readCalibrationXML() {
    cv::FileStorage fs(this->calibrationUrl, cv::FileStorage::READ); // Read the settings
    std::cerr << "URL: " << calibrationUrl << "\n";
    if (!fs.isOpened())
    {
        std::cerr << "Could not open the configuration file" << "\n";
        exit(EXIT_FAILURE);
        return false;
    }
    cv::FileNode cameraMatrixNode = fs["camera_matrix"];
    cameraMatrixNode >> this->camMatrix;
    cv::FileNode cameraDistortionNode = fs["distortion_coefficients"];
    cameraDistortionNode >> this->distCoeff;
    int width, height;
    fs["image_width"] >> width;
    fs["image_height"] >> height;
    this->resolution = cv::Size(width, height);
    fs.release(); // close Settings file

    //std::cout << this->camMatrix << std::endl;
    //std::cout << this->distCoeff << std::endl;

    return true;
}


void Camera::setCamMatrix(std::vector<double> data, int rows, int cols){
    //check matrix size
    if ((rows != 3) || (cols != 3)){
        std::cerr << "ERROR: expected 3x3 matrix, got " << rows << "x" << cols << "! exiting\n";
        exit(EXIT_FAILURE);
    }
    camMatrix = cv::Mat(rows, cols, CV_64F);

    for(int i=0;i<camMatrix.size().width;i++){
        for(int j=0;j<camMatrix.size().height;j++){
            camMatrix.at<double>(i,j) = data.at(i*camMatrix.size().height+j);
        }
    }
    std::cout << this->camMatrix << std::endl;

}

void Camera::setDistCoeff(std::vector<double> data, int rows, int cols){
    //check rows and cols
    if ((rows != 5) || (cols != 1)){
        std::cerr << "ERROR: expected 5x1 matrix, got " << rows << "x" << cols << "! exiting\n";
        exit(EXIT_FAILURE);
    }

    //set matrix
    distCoeff = cv::Mat(rows, cols, CV_64F);

    for(int i=0;i<distCoeff.size().width;i++){
        for(int j=0;j<distCoeff.size().height;j++){
            distCoeff.at<double>(i,j) = data.at(i*distCoeff.size().height+j);
        }
    }
    std::cout << this->distCoeff << std::endl;
}


bool
Camera::readCalibrationYAML() {
    //cv::FileStorage fs(this->calibrationUrl, cv::FileStorage::READ); // Read the settings
    std::cerr << "YAML URL: " << calibrationUrl << "\n";

#ifndef YAML_CPP_AVAILABLE
    std::cerr << "ERROR: bart was compiled without yaml-cpp support - could not open yaml. exiting...";
    exit(EXIT_FAILURE);
#else
    YAML::Node config = YAML::LoadFile(calibrationUrl);

    setCamMatrix(
        config["camera_matrix"]["data"].as<std::vector<double>>(),
        config["camera_matrix"]["rows"].as<int>(),
        config["camera_matrix"]["cols"].as<int>());

    //NOTE: ros gives us rows and cols swapped!
    int dc_rows = config["distortion_coefficients"]["rows"].as<int>();
    int dc_cols = config["distortion_coefficients"]["cols"].as<int>();
    if (dc_rows != 5){
        //SWAP!
        int tmp = dc_cols;
        dc_cols = dc_rows;
        dc_rows = tmp;
    }
    setDistCoeff(
        config["distortion_coefficients"]["data"].as<std::vector<double>>(),
        dc_rows,
        dc_cols);


    this->resolution = cv::Size(config["image_width"].as<int>(), config["image_height"].as<int>());

    //this->name = config["camera_name"].as<std::string>();

    //std::cout << this->camMatrix << std::endl;
    //std::cout << this->distCoeff << std::endl;
#endif

    return true;
}
