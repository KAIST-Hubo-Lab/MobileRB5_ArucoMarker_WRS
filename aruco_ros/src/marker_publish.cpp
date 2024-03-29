/*****************************
Copyright 2011 Rafael Muñoz Salinas. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED BY Rafael Muñoz Salinas ''AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL Rafael Muñoz Salinas OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the
authors and should not be interpreted as representing official policies, either expressed
or implied, of Rafael Muñoz Salinas.
********************************/
/**
* @file marker_publish.cpp
* @author Bence Magyar
* @date June 2014
* @brief Modified copy of simple_single.cpp to publish all markers visible
* (modified by Josh Langsfeld, 2014)
*/

#include <iostream>
#include <aruco/aruco.h>
#include <aruco/cvdrawingutils.h>

#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <aruco_ros/aruco_ros_utils.h>
#include <aruco_msgs/MarkerArray.h>
#include <tf/transform_listener.h>
#include <std_msgs/UInt32MultiArray.h>

//pointcloud
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/point_cloud2_iterator.h>

int boxX = 0;
int boxY = 0;
//variable to compare left/right marker distance from RGB and Pcloud
float leftMarker_rgb = 0;
float rightMarker_rgb = 0;
float leftMarker_pcloud = 0;
float rightMarker_pcloud = 0;

sensor_msgs::PointCloud2 pcloud_data;

void cloud_cb (sensor_msgs::PointCloud2ConstPtr const& input)
{

    //copy input to global output
    pcloud_data = *input;

    /*
    geometry_msgs::PointStamped base_point;
    static tf::TransformListener listner;
    //std::cout << "=== handling pointCloud === " << std::endl;

    int width = input->width;
    int height = input->height;
    // printf("cloud width: %u, height: %u, rowstep: %u, pointstep: %u\n" , input->width, input->height, input->row_step, input->point_step);
    int arrayPosition = boxY*input->row_step + boxX*input->point_step;
    // printf("position: %u\n" , arrayPosition);

    // compute position in array where x,y,z data start
    int arrayPosX = arrayPosition + input->fields[0].offset; // X has an offset of 0
    int arrayPosY = arrayPosition + input->fields[1].offset; // Y has an offset of 4
    int arrayPosZ = arrayPosition + input->fields[2].offset; // Z has an offset of 8

    float X = 0.0;
    float Y = 0.0;
    float Z = 0.0;

    memcpy(&X, &input->data[arrayPosX], sizeof(float));
    memcpy(&Y, &input->data[arrayPosY], sizeof(float));
    memcpy(&Z, &input->data[arrayPosZ], sizeof(float));

    printf("x: %f, y: %f, z: %f\n" , X, Y, Z);
    */
}

class ArucoMarkerPublisher
{
private:
  // aruco stuff
  aruco::MarkerDetector mDetector_;
  aruco::CameraParameters camParam_;
  vector<aruco::Marker> markers_;

  // node params
  bool useRectifiedImages_;
  std::string marker_frame_;
  std::string camera_frame_;
  std::string reference_frame_;
  double marker_size_;
  bool rotate_marker_axis_;

  // ROS pub-sub
  ros::NodeHandle nh_;
  image_transport::ImageTransport it_;
  image_transport::Subscriber image_sub_;

  image_transport::Publisher image_pub_;
  image_transport::Publisher debug_pub_;
  ros::Publisher marker_pub_;
  ros::Publisher marker_pub_cloud;

  ros::Publisher marker_list_pub_;
  tf::TransformListener tfListener_;

  ros::Subscriber cam_info_sub_;

  aruco_msgs::MarkerArray::Ptr marker_msg_;
  aruco_msgs::MarkerArray::Ptr marker_pcloud_msg_;
  cv::Mat inImage_;
  bool useCamInfo_;
  std_msgs::UInt32MultiArray marker_list_msg_;




public:
  ArucoMarkerPublisher()
    : nh_("~")
    , it_(nh_)
    , useCamInfo_(true)
  {
    image_sub_ = it_.subscribe("/image", 1, &ArucoMarkerPublisher::image_callback, this);



    nh_.param<bool>("use_camera_info", useCamInfo_, true);
    if(useCamInfo_)
    {
      sensor_msgs::CameraInfoConstPtr msg = ros::topic::waitForMessage<sensor_msgs::CameraInfo>("/camera_info", nh_);//, 10.0);
      camParam_ = aruco_ros::rosCameraInfo2ArucoCamParams(*msg, useRectifiedImages_);
      nh_.param<double>("marker_size", marker_size_, 0.05);
      nh_.param<bool>("image_is_rectified", useRectifiedImages_, true);
      nh_.param<std::string>("reference_frame", reference_frame_, "");
      nh_.param<std::string>("camera_frame", camera_frame_, "");
      nh_.param<bool>("rotate_marker_axis", rotate_marker_axis_, true);
      ROS_ASSERT(not (camera_frame_.empty() and not reference_frame_.empty()));
      if(reference_frame_.empty())
        reference_frame_ = camera_frame_;
    }
    else
    {
      camParam_ = aruco::CameraParameters();
    }

    image_pub_ = it_.advertise("result", 1);
    debug_pub_ = it_.advertise("debug", 1);
    marker_pub_ = nh_.advertise<aruco_msgs::MarkerArray>("markers", 100);
    marker_pub_cloud = nh_.advertise<aruco_msgs::MarkerArray>("markers_pcloud", 100);
    marker_list_pub_ = nh_.advertise<std_msgs::UInt32MultiArray>("markers_list", 10);

    marker_msg_ = aruco_msgs::MarkerArray::Ptr(new aruco_msgs::MarkerArray());
    marker_msg_->header.frame_id = reference_frame_;
    marker_msg_->header.seq = 0;

    marker_pcloud_msg_ =  aruco_msgs::MarkerArray::Ptr(new aruco_msgs::MarkerArray());
    marker_pcloud_msg_->header.frame_id = reference_frame_;
    marker_pcloud_msg_->header.seq = 0;

  }

  bool getTransform(const std::string& refFrame,
                    const std::string& childFrame,
                    tf::StampedTransform& transform)
  {
    std::string errMsg;

    if(!tfListener_.waitForTransform(refFrame,
                                     childFrame,
                                     ros::Time(0),
                                     ros::Duration(0.5),
                                     ros::Duration(0.01),
                                     &errMsg))
    {
      ROS_ERROR_STREAM("Unable to get pose from TF: " << errMsg);
      return false;
    }
    else
    {
      try
      {
        tfListener_.lookupTransform(refFrame, childFrame,
                                    ros::Time(0),        //get latest available
                                    transform);
      }
      catch ( const tf::TransformException& e)
      {
        ROS_ERROR_STREAM("Error in lookupTransform of " << childFrame << " in " << refFrame);
        return false;
      }

    }
    return true;
  }

  void image_callback(const sensor_msgs::ImageConstPtr& msg)
  {
      bool publishMarkers = marker_pub_.getNumSubscribers() > 0;
      bool publishMarkersList = marker_list_pub_.getNumSubscribers() > 0;
      bool publishImage = image_pub_.getNumSubscribers() > 0;
      bool publishDebug = debug_pub_.getNumSubscribers() > 0;

      if(!publishMarkers && !publishMarkersList && !publishImage && !publishDebug)
        return;

      ros::Time curr_stamp(ros::Time::now());
      cv_bridge::CvImagePtr cv_ptr;
      try
      {
        cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::RGB8);
        inImage_ = cv_ptr->image;

        //clear out previous detection results
        markers_.clear();

        //Ok, let's detect
        mDetector_.detect(inImage_, markers_, camParam_, marker_size_, false);

        // marker array publish
        if(publishMarkers)
        {
            marker_msg_->markers.clear();
            marker_msg_->markers.resize(markers_.size());
            marker_msg_->header.stamp = curr_stamp;
            marker_msg_->header.seq++;


            marker_pcloud_msg_->markers.clear();
            marker_pcloud_msg_->markers.resize(markers_.size());
            marker_pcloud_msg_->header.stamp = curr_stamp;
            marker_pcloud_msg_->header.seq++;




          for(size_t i=0; i<markers_.size(); ++i)
          {
            aruco_msgs::Marker & marker_i = marker_msg_->markers.at(i);
            marker_i.header.stamp = curr_stamp;
            marker_i.id = markers_.at(i).id;
            marker_i.confidence = 1.0;


            aruco_msgs::Marker & marker_i_plcoud = marker_pcloud_msg_->markers.at(i);
            marker_i_plcoud.header.stamp = curr_stamp;
            marker_i_plcoud.id = markers_.at(i).id;
            marker_i_plcoud.confidence = 1.0;

          }

          // if there is camera info let's do 3D stuff
          if(useCamInfo_)
          {
            //get the current transform from the camera frame to output ref frame
            tf::StampedTransform cameraToReference;
            cameraToReference.setIdentity();

            if ( reference_frame_ != camera_frame_ )
            {
              getTransform(reference_frame_,
                  camera_frame_,
                  cameraToReference);
            }

            //Now find the transform for each detected marker
            for(size_t i=0; i<markers_.size(); ++i)
            {
              aruco_msgs::Marker & marker_i = marker_msg_->markers.at(i);
              aruco_msgs::Marker & marker_i_plcoud = marker_pcloud_msg_->markers.at(i);


              tf::Transform transform = aruco_ros::arucoMarker2Tf(markers_[i], rotate_marker_axis_);
              transform = static_cast<tf::Transform>(cameraToReference) * transform;

              tf::poseTFToMsg(transform, marker_i.pose.pose);
              marker_i.header.frame_id = reference_frame_;

              tf::poseTFToMsg(transform, marker_i_plcoud.pose.pose);
              marker_i_plcoud.header.frame_id = reference_frame_;




              //LMY get pixel
              //ROS_INFO("marker: %d, x: %f, y: %f\n",i, markers_[i].getCenter().x, markers_[i].getCenter().y);
              boxX = int(markers_[i].getCenter().x);
              boxY = int(markers_[i].getCenter().y);



              int width = pcloud_data.width;
              int height = pcloud_data.height;
              // printf("cloud width: %u, height: %u, rowstep: %u, pointstep: %u\n" , input->width, input->height, input->row_step, input->point_step);
              int arrayPosition = boxY*pcloud_data.row_step + boxX*pcloud_data.point_step;
              // printf("position: %u\n" , arrayPosition);

              // compute position in array where x,y,z data start
              int arrayPosX = arrayPosition + pcloud_data.fields[0].offset; // X has an offset of 0
              int arrayPosY = arrayPosition + pcloud_data.fields[1].offset; // Y has an offset of 4
              int arrayPosZ = arrayPosition + pcloud_data.fields[2].offset; // Z has an offset of 8

              float X = 0.0;
              float Y = 0.0;
              float Z = 0.0;

              memcpy(&X, &pcloud_data.data[arrayPosX], sizeof(float));
              memcpy(&Y, &pcloud_data.data[arrayPosY], sizeof(float));
              memcpy(&Z, &pcloud_data.data[arrayPosZ], sizeof(float));

              //ROS_INFO("ID: %d, x: %f, y: %f, z: %f\n" , markers_[i].id, X, Y, Z);
/*
              if(markers_[i].id == 1)
              {
                  leftMarker_rgb = marker_i.pose.pose.position.x;
                  leftMarker_pcloud = X;
              }

              if(markers_[i].id == 5)
              {
                  rightMarker_rgb = marker_i.pose.pose.position.x;
                  rightMarker_pcloud = X;
              }
*/


              //marker_i_plcoud.pose.pose.position.x = X;
              //marker_i_plcoud.pose.pose.position.y = Y;
              //marker_i_plcoud.pose.pose.position.z = Z;

              //marker_i.pose.pose.position.x = X;
              //marker_i.pose.pose.position.y = Y;
              //marker_i.pose.pose.position.z = Z;

            }
          }

          //publish marker array
          if (marker_msg_->markers.size() > 0)
          {
            marker_pub_.publish(marker_msg_);
            marker_pub_cloud.publish(marker_pcloud_msg_);
            }


        }

        //ROS_INFO("RGB gap: %f, Pcloud gap: %f", rightMarker_rgb-leftMarker_rgb, rightMarker_pcloud-leftMarker_pcloud);

        if(publishMarkersList)
        {
            marker_list_msg_.data.resize(markers_.size());
            for(size_t i=0; i<markers_.size(); ++i)
              marker_list_msg_.data[i] = markers_[i].id;

            marker_list_pub_.publish(marker_list_msg_);
        }

        // Draw detected markers on the image for visualization
        for(size_t i=0; i<markers_.size(); ++i)
        {

          markers_[i].draw(inImage_,cv::Scalar(0,0,255),2);
        }


        //draw a 3d cube in each marker if there is 3d info
        if(camParam_.isValid() && marker_size_>0)
        {
          for(size_t i=0; i<markers_.size(); ++i)
            aruco::CvDrawingUtils::draw3dAxis(inImage_, markers_[i], camParam_);
        }

        // publish input image with markers drawn on it
        if(publishImage)
        {
          //show input with augmented information
          cv_bridge::CvImage out_msg;
          out_msg.header.stamp = curr_stamp;
          out_msg.encoding = sensor_msgs::image_encodings::RGB8;
          out_msg.image = inImage_;
          image_pub_.publish(out_msg.toImageMsg());
        }

        // publish image after internal image processing
        if(publishDebug)
        {
          //show also the internal image resulting from the threshold operation
          cv_bridge::CvImage debug_msg;
          debug_msg.header.stamp = curr_stamp;
          debug_msg.encoding = sensor_msgs::image_encodings::MONO8;
          debug_msg.image = mDetector_.getThresholdedImage();
          debug_pub_.publish(debug_msg.toImageMsg());
        }

      }
      catch (cv_bridge::Exception& e)
      {
        ROS_ERROR("cv_bridge exception: %s", e.what());
      }
  }
};


int main(int argc,char **argv)
{
  ros::init(argc, argv, "aruco_marker_publisher");
  ros::NodeHandle nh;
  ros::Subscriber sub_pointcloud = nh.subscribe ("/camera/depth_registered/points", 1, cloud_cb);


  ArucoMarkerPublisher node;

  ros::spin();
}
