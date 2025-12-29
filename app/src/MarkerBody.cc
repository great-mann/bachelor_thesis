
#include "MarkerBody.h"



void
MarkerBody::write(cv::FileStorage& fs) const //Write serialization for this class
{
    fs << "{" << "name" << name
            << "isStatic" << isStatic
            << "markers" << "[";

    for (std::map<int, MarkerTransform>::const_iterator m = markers.begin(); m != markers.end(); ++m) {
        fs << "{"
           << "id"   << m->second.id 
           << "size" << m->second.size 
           << "position"    << m->second.getPosition()
           << "orientation" << m->second.getOrientation()
           << "}";
    }

    fs << "}";
}

void 
MarkerBody::read(const cv::FileNode& node) //Read serialization for this class
{
    name = (std::string)node["name"];
    isStatic = (bool)(int)node["isStatic"];
    cv::FileNode markerNode = node["markers"];
    for (cv::FileNodeIterator m = markerNode.begin(); m != markerNode.end(); ++m) {
        int id = (int)(*m)["id"];
        float size = 1000.0f * (float)(*m)["size"];
        std::vector<float> posVec;
        std::vector<float> oriVec;
        (*m)["position"] >> posVec;
        (*m)["orientation"] >> oriVec;
        markers[id] = MarkerTransform(id, cv::Size2f(size,size), GeometryHelpers::constructMat(
                                                                  cv::Vec3f(posVec[0],posVec[1],posVec[2]),
                                                                  cv::Vec4f(oriVec[0],oriVec[1],oriVec[2],oriVec[3])));
    }
}
