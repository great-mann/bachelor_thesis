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

#include "KalmanMarker.h"
#include "DebugHelpers.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>
using namespace cv;
#define KALMAN_VELOCITY 1

KalmanMarker::KalmanMarker(Marker _marker, boost::posix_time::ptime timestamp){
    marker = _marker;
    coord = marker.get_center();
    id = marker.id;

    timestamp_last_updated = timestamp;
    timestamp_last_frame = timestamp;
    not_seen_counter = 0;
    //time between measurements, assumes 50 fps
    double dt = 1.0 / 1.0; //strange, does work best when dt=1.0?!

#if !KALMAN_VELOCITY
    kalman_filter = new KalmanFilter(4, 2, 0);
    //use position and velocity:
    kalman_filter->transitionMatrix = *(Mat_<float>(4, 4) << 1,0,dt,0,   0,1,0,dt,  0,0,1,0,  0,0,0,1);
    kalman_filter->statePre.at<float>(0) = coord.x;
    kalman_filter->statePre.at<float>(1) = coord.y;
    kalman_filter->statePre.at<float>(2) = 0.0;
    kalman_filter->statePre.at<float>(3) = 0.0;
    setIdentity(kalman_filter->measurementMatrix);

#else
    //better motion estimation
    kalman_filter = new KalmanFilter(6, 2, 0);
    //use position and velocity:
    kalman_filter->transitionMatrix = (Mat_<float>(6, 6) <<  \
                                        1,0,dt,0,0.5*pow(dt,2),0, \
                                        0,1,0,dt,0,0.5*pow(dt,2), \
                                        0,0,1,0,dt,0, \
                                        0,0,0,1,0,dt, \
                                        0,0,0,0,1,0, \
                                        0,0,0,0,0,1);
    kalman_filter->statePre.at<float>(0) = coord.x;
    kalman_filter->statePre.at<float>(1) = coord.y;
    kalman_filter->statePre.at<float>(2) = 0.0;
    kalman_filter->statePre.at<float>(3) = 0.0;
    kalman_filter->statePre.at<float>(4) = 0.0;
    kalman_filter->statePre.at<float>(5) = 0.0;
    kalman_filter->measurementMatrix = (Mat_<float>(2, 6) << 1,0,1,0,0.5,0, 0,1,0,1,0,0.5);

#endif

    //kalman coefficients
    set_kalman_coefficients(0.001, 0.2, 0.2);
    Mat_<float> measurement(2,1);
    measurement.setTo(Scalar(0));
    measurement(0) = coord.x;
    measurement(1) = coord.y;

    //initialise (?)
    kalman_filter->correct(measurement);
}

//this will always return the most up to date position
//this is either an estimated (future) position from last frame
//or (if set in this frame) the actual position
Marker KalmanMarker::get_marker(boost::posix_time::ptime timestamp){
    Marker result = marker;

    Point2f motion_vector =  coord - result.get_center();
    result.translate(motion_vector * 1.0);

    //std::cout << "KALMAN2 " << kalman_filter->statePre << " post " << kalman_filter->statePost << "\n";

    return result;
}


KalmanMarker::KalmanMarker(){
    id = -1;
    #if !KALMAN_VELOCITY
    kalman_filter = new KalmanFilter(4, 2, 0);
    #else
    kalman_filter = new KalmanFilter(6, 2, 0);
    #endif
}

bool KalmanMarker::is_alive(){
    //if not seen > n frames we say the item was lost
    //printf("ALIVE(%4d) %d\n",id,not_seen_counter);
    if (not_seen_counter > 50){
        return false;
    }else{
        if (marker.id == -1){
            return false;
        }else{
            return true;
        }
    }
}


void KalmanMarker::update_marker(Marker _marker, boost::posix_time::ptime timestamp){
    if (_marker.id == -1){
        //printf("WARNING: KALMAN ignoring marker with id -1!\n");
        return;
    }

    if (timestamp <= timestamp_last_updated){
        //we have newer info than the incoming data, return
        //printf("KALMAN TS UP FAIL: timestamp <= timestamp_last_updated\n");
        return;
    }

    //incoming update
    marker = _marker;
    coord = marker.get_center();
    //printf("KALMAN UPDATE %d entry %f %f\n",id, coord.x,coord.y);

    marker_updated = true;
    not_seen_counter = 0;
    timestamp_last_updated = timestamp;
}


void KalmanMarker::next_frame(boost::posix_time::ptime timestamp){
    //printf("KALMAN NF %d entry %f %f (UpDATED=%s)\n",id, coord.x,coord.y,marker_updated?"TRUE":"FALSE");
    if (timestamp <= timestamp_last_frame){
        //we have newer info than the incoming data, return
        //printf("KALMAN NF -> not updated, old frame\n");
        return;
    }
    timestamp_last_updated = timestamp;
    timestamp_last_frame = timestamp;

    //always execute kalman predict phase:
    Mat prediction = kalman_filter->predict();


    //kalman_filter->statePre.copyTo(kalman_filter->statePost);
    //kalman_filter->errorCovPre.copyTo(kalman_filter->errorCovPost);


    if (marker_updated){
        //new measurement available -> update kalman:
        Mat_<float> measurement(2,1);
        measurement.setTo(Scalar(0));
        measurement(0) = coord.x;
        measurement(1) = coord.y;
        Mat estimated = kalman_filter->correct(measurement);
        coord = cv::Point2f(estimated.at<float>(0), estimated.at<float>(1));
        marker_updated = false;
        //printf("KALMAN %d NEW EXSTIMATE  %f %f\n",id,coord.x,coord.y);
    }else{
        //oh, we did not track this item in the current frame! count it!
        not_seen_counter++;
        //save last prediction
        coord = cv::Point2f(prediction.at<float>(0),prediction.at<float>(1));
        //printf("KALMAN %d NEW PREDICTION  %f %f\n",id,coord.x,coord.y);
    }

    //now comes the important step: we want to look one
    //step ahead into the future.
    //we do this manually as we DO NOT want to update
    //the internal kalman states (we dont want to mess up the kalman state
    //in case we get a measurement in next frame)
    Mat future_prediction = kalman_filter->predict(); //kalman_filter->statePost;
    //future_prediction = kalman_filter->transitionMatrix * future_prediction;
    coord.x = future_prediction.at<float>(0);
    coord.y = future_prediction.at<float>(1);

    //printf("KALMAN NF %d exit %f %f\n",id,coord.x,coord.y);
}


void KalmanMarker::set_kalman_coefficients(double p, double m, double e){
        options_kalman_process_noise_cov = p;
        options_kalman_measurement_noise_cov = m;
        options_kalman_error_cov = e;

        setIdentity(kalman_filter->processNoiseCov, Scalar::all(options_kalman_process_noise_cov));
        setIdentity(kalman_filter->measurementNoiseCov, Scalar::all(options_kalman_measurement_noise_cov));
        setIdentity(kalman_filter->errorCovPost, Scalar::all(options_kalman_error_cov));
}

//int main(int argc, char **argv){
//    float pos;
//    //printf("TESTING KALMAN");
//    Marker m(123);
//    m.points.push_back(Point2f(0,0));
//    m.points.push_back(Point2f(0,0));
//    m.points.push_back(Point2f(0,0));
//    m.points.push_back(Point2f(0,0));
//
//    KalmanMarker km(m, boost::posix_time::microsec_clock::local_time());
//    boost::this_thread::sleep(boost::posix_time::microseconds(1000));
//
//    for(pos = 100; pos<1000; pos += 5){
//        m.points[0] = Point2f(pos,pos) + Point2f(-10,-10);
//        m.points[1] = Point2f(pos,pos) + Point2f(+10,-10);
//        m.points[2] = Point2f(pos,pos) + Point2f(-10,+10);
//        m.points[3] = Point2f(pos,pos) + Point2f(+10,+10);
//        //printf("\nNOW %06.1f\n",pos);
//        km.update_marker(m, boost::posix_time::microsec_clock::local_time());
//        km.next_frame(boost::posix_time::microsec_clock::local_time());
//        boost::this_thread::sleep(boost::posix_time::microseconds(1000));
//    }
//    for(int i=0; i<10; i++){
//        km.next_frame(boost::posix_time::microsec_clock::local_time());
//        boost::this_thread::sleep(boost::posix_time::microseconds(1000));
//        Point2f now = km.get_current_position();
//        //printf("> ESTIMATE %3d = %06.2f (should be %06.1f)\n",i,now.x,pos);
//        pos +=5;
//    }
//}
