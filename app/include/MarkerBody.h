/* 
 * File:   MarkerBody.h
 * Author: prenner
 *
 * Created on June 27, 2014, 3:12 PM
 */

#ifndef MARKERBODY_H
#define	MARKERBODY_H

#include "GeometryHelpers.h"

#include <opencv2/opencv.hpp>

/**
 * Struct holding the transformation information of a marker.
 */
struct MarkerTransform {
    int id;
    cv::Size2f size;
    cv::Mat matrix;
    
    MarkerTransform() {}
    
    MarkerTransform(int id, cv::Size2f size, cv::Mat matrix) {
        this->id = id;
        this->size = size;
        this->matrix = matrix;
    } 
    
    cv::Vec3f getPosition() const {
        return cv::Vec3f(matrix.at<float>(3,0), matrix.at<float>(3,1), matrix.at<float>(3,2));
    }
    
    cv::Vec4f getOrientation() const {
        cv::Vec4f orientation;
        GeometryHelpers::decomposeMat(this->matrix, orientation);
        return orientation;
    }
    
};


/**
 * Struct holding information about single markers.
 */
struct SingleMarker {
    
    int id;
    float size;
    bool isStatic;
    
    SingleMarker() {}
    
    SingleMarker(int id, float size, bool isStatic) {
        this->id = id;
        this->size = size;
        this->isStatic = isStatic;
    }
    
};


/**
 * Struct holding the transformation information of a marker body.
 */
struct MarkerBody {
    
    std::string name;
    std::map<int, MarkerTransform> markers;
    bool isStatic;
    
    MarkerBody() {
        name = "";
        isStatic = false;
    }
    
    
    void write(cv::FileStorage& fs) const;
    void read(const cv::FileNode& node); 
};
static void 
write(cv::FileStorage& fs, const std::string&, const MarkerBody& body)
{
    body.write(fs);
}

static void 
read(const cv::FileNode& node, MarkerBody& body, const MarkerBody& default_value = MarkerBody())
{
if(node.empty())
    body = default_value;
else
    body.read(node);
}

#endif	/* MARKERBODY_H */

