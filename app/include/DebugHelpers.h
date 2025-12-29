#ifndef DEBUG_HELPERS_H
#define DEBUG_HELPERS_H

#include <string>
#include <sstream>
#include <iostream>

#include <opencv2/highgui/highgui.hpp>



template <typename T>
std::string ToString(const T& value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}


class InstantIOLogger
{

public:
	
	static void addLog(std::string s);

	static std::string flushLogStr();

private:
	static std::stringstream logStringStream;

};

namespace cv
{
    inline void showAndSave(std::string name, const cv::Mat& m)
    {
        cv::imshow(name, m);
        cv::imwrite(name + ".png", m);
    }
}


#endif //DEBUG_HELPERS_H