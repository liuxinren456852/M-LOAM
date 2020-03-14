#include <ros/ros.h>
#include <nav_msgs/Odometry.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/NavSatFix.h>

#include <std_msgs/Header.h>
#include <sensor_msgs/PointCloud2.h>

#include <message_filters/sync_policies/approximate_time.h>
#include <message_filters/subscriber.h>

// PCL
#include <pcl/point_cloud.h>			/* pcl::PointCloud */
#include <pcl/point_types.h>			/* pcl::PointXYZ */
#include <pcl/filters/voxel_grid.h>
#include <pcl/common/transforms.h>
#include <pcl_conversions/pcl_conversions.h>

#include <iostream>

using namespace std;

typedef sensor_msgs::PointCloud2 LidarMsgType;
typedef message_filters::sync_policies::ApproximateTime<LidarMsgType, LidarMsgType> LidarSyncPolicy;
typedef message_filters::Subscriber<LidarMsgType> LidarSubType;

std::vector<double> cloud_time_list;
string data_path;

queue<nav_msgs::OdometryConstPtr> gt_odom_buf;
queue<sensor_msgs::NavSatFixConstPtr> gps_buf;


void process(const sensor_msgs::PointCloud2ConstPtr& pc2_top,
             const sensor_msgs::PointCloud2ConstPtr& pc2_front,
             const sensor_msgs::PointCloud2ConstPtr& pc2_left,
             const sensor_msgs::PointCloud2ConstPtr& pc2_right)             
{
    pcl::PointCloud<pcl::PointXYZ> cloud;
    double cloud_time = pc2_top->header.stamp.toSec;
    cloud_time_list.push_back(cloud_time);
    size_t frame_cnt = cloud_time_list.size() - 1;
    {
        stringstream ss;
        ss << data_path << "cloud_0/" << setfill('0') << setw(6) << frame_cnt << ".pcd";
        pcl::fromROSMsg(*pc2_top, cloud);
        pcl::io::savePCDFileASCII(ss.str(), cloud);
    }
    {
        stringstream ss;
        ss << data_path << "cloud_1/" << setfill('0') << setw(6) << frame_cnt << ".pcd";
        pcl::fromROSMsg(*pc2_front, cloud);
        pcl::io::savePCDFileASCII(ss.str(), cloud);
    }    
    {
        stringstream ss;
        ss << data_path << "cloud_2/" << setfill('0') << setw(6) << frame_cnt << ".pcd";
        pcl::fromROSMsg(*pc2_left, cloud);
        pcl::io::savePCDFileASCII(ss.str(), cloud);
    }    
    {
        stringstream ss;
        ss << data_path << "cloud_3/" << setfill('0') << setw(6) << frame_cnt << ".pcd";
        pcl::fromROSMsg(*pc2_right, cloud);
        pcl::io::savePCDFileASCII(ss.str(), cloud);
    }

    {
        while (!gps_buf.empty() && gps_buf.top()->header.stamp.toSec() < cloud_time)
        {
            gps_buf.pop();
        }
        stringstream ss;
        ss << data_path << "gps/" << setfill('0') << setw(6) << frame_cnt << ".txt";
        ofstream gps_file(ss.str());
        sensor_msgs::NavSatFix gps_position = gps_buf.top();
        gps_file << gps_position->status.status << " "
                 << gps_position->status.service << " "
                 << gps_position->latitude << " "
                 << gps_position->longitude << " "
                 << gps_position->altitude << " "
                 << gps_position->position_covariance[0] << " "
                 << gps_position->position_covariance[4] << " "
                 << gps_position->position_covariance[8] << std::endl;
        gps_file.close();
        gps_buf.pop();
    }    


    {
        while (!gt_odom_buf.empty() && gt_odom_buf.top()->header.stamp.toSec() < cloud_time)
        {
            gt_odom_buf.pop();
        }
        stringstream ss;
        ss << data_path << "gt_odom/" << setfill('0') << setw(6) << frame_cnt << ".txt";
        ofstream gt_odom_file(ss.str());
        nav_msgs::Odometry gt_odom = gt_odom_buf.top();
        gt_odom_file << gt_odom->pose.pose.position.x << " "
                     << gt_odom->pose.pose.position.y << " "
                     << gt_odom->pose.pose.position.z << " "
                     << gt_odom->pose.pose.orientation.x << " "
                     << gt_odom->pose.pose.orientation.y << " "
                     << gt_odom->pose.pose.orientation.z << " "
                     << gt_odom->pose.pose.orientation.w << std::endl;
        gt_odom_file.close();
        gt_odom_buf.pop();
    }    
}

void gps_callback(sensor_msgs::NavSatFixConstPtr &gps_msg)
{
    gps_buf.push(gps_msg);
}

void gt_odom_callback(nav_msgs::OdometryConstPtr &odom_msg)
{
    gt_odom_buf.push(odom_msg);
}

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        printf("please intput: rosrun mlod test_parse_bag  \n"
                "for example: "
                "rosrun mloam test_parse_bag "
                "/Monster/dataset/UDI_dataset/xfl_20200301/short_test/\n");
        return 1;
    }

    ros::init(argc, argv, "test_merge_pointcloud");
    ros::NodeHandle nh("~");

    data_path = argv[1];

    //register fusion callback function

    ros::Subscriber sub_gt_odom = nh.subscribe("/current_odom", 100, gt_odom_callback);
    ros::Subscriber sub_gps = nh.subscribe("/novatel718d/pos", 100, gps_callback);

    LidarSubType* sub_top = new LidarSubType(nh, "/top/rslidar_points", 1);
    LidarSubType* sub_front = new LidarSubType(nh, "/front/rslidar_points", 1);
    LidarSubType* sub_left = new LidarSubType(nh, "/left/rslidar_points", 1);
    LidarSubType* sub_right = new LidarSubType(nh, "/right/rslidar_points", 1);
    message_filters::Synchronizer<LidarSyncPolicy>* lidar_synchronizer =
        new message_filters::Synchronizer<LidarSyncPolicy>(
            LidarSyncPolicy(10), *sub_top, *sub_front, *sub_left, *sub_right);
    lidar_synchronizer->registerCallback(boost::bind(&process, _1, _2, _3, _4));

    ros::Rate fps(100);
    while (ros::ok())
	{
        ros::spinOnce();
        fps.sleep();
    }

    ofstream cloud_time_file(std::string(data_path + "cloud_0/timestamps.txt").c_str());
    cloud_time_file.setf(ios::fixed, ios::floatfield);
    for (size_t i = 0; i < cloud_time_list; i++)
    {
        cloud_time_file.precision(15);
        cloud_time_file << cloud_time_list[i] << std::endl;
    }
    cloud_time_file.close();

    return 0;
}
