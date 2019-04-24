#include "Apriltag2Process.h"


#include <cmath>

#include <google/protobuf/util/delimited_message_util.h>

#include <asio/write.hpp>
#include <asio/connect.hpp>
#include <asio/posix/stream_descriptor.hpp>


#include <glog/logging.h>
#include <opencv2/imgcodecs.hpp>


#include "utils/PosixCall.h"
#include "utils/Partitions.h"


#include <Eigen/Geometry>
#include <Eigen/SVD>


AprilTag2Detector::AprilTag2Detector(size_t maxWorkers,
	                                 const AprilTag2Detector::Config & config)
{
	d_detectors.reserve(maxWorkers);
	for (size_t i = 0 ; i < maxWorkers; ++i ) {
		auto f = maytags::Family::Create(config.Family);
		maytags::Detector::Config mConfig;
		mConfig.DecodeSharpening = 0.25;
		mConfig.QuadConfig.MinimumBWDifference = config.QuadMinBWDiff;
		mConfig.QuadConfig.MaxLineMSE = config.QuadMaxLineMSE;
		mConfig.QuadConfig.CriticalCornerAngleRadian = config.QuadCriticalRadian;
		mConfig.QuadConfig.MaximumNumberOfMaxima = config.QuadMaxNMaxima;
		mConfig.QuadConfig.MinimumPixelPerCluster = config.QuadMinClusterPixel;
		mConfig.Families.push_back(f);
		d_detectors.push_back(std::make_shared<maytags::Detector>(mConfig));
	}
}

AprilTag2Detector::~AprilTag2Detector() {}



AprilTag2Detector::ROITagDetection::ROITagDetection(const AprilTag2Detector::Ptr & parent)
	: d_parent(parent ) {
}

AprilTag2Detector::ROITagDetection::~ROITagDetection() {}

double ComputeAngleFromCorner(const maytags::Detection & d) {
	Eigen::Vector2d delta = (d.Corners[0] + d.Corners[2]) / 2.0 - (d.Corners[1] + d.Corners[3]) / 2.0;

	return atan2(delta.y(),delta.x());
}

// double ComputeAngleFromHomography(apriltag_detection *q) {
// 	typedef Eigen::Transform<double,2,Eigen::Projective> Homography;
// 	Homography hm;
// 	Eigen::Matrix2d rotation;
// 	Eigen::Matrix2d scaling;
// 	//angle estimation
// 	hm.matrix() <<
// 		q->H->data[0], q->H->data[1], q->H->data[2],
// 		q->H->data[3], q->H->data[4], q->H->data[5],
// 		q->H->data[6], q->H->data[7], q->H->data[8];

// 	hm.computeRotationScaling(&rotation,&scaling);
// 	return Eigen::Rotation2D<double>(rotation).angle();
// }


std::vector<ProcessFunction> AprilTag2Detector::ROITagDetection::Prepare(size_t maxProcess, const cv::Size & size ) {
	maxProcess = std::min(maxProcess,d_parent->d_detectors.size());

	d_parent->d_results.resize(maxProcess);
	size_t margin = 75;
	Partition partitions;
	PartitionRectangle(cv::Rect(cv::Point(0,0),size),maxProcess,partitions);
	AddMargin(size,margin,partitions);

	std::vector<ProcessFunction> toReturn;
	toReturn.reserve(maxProcess);
	for (size_t i = 0; i < maxProcess; ++i) {
		d_parent->d_results.reserve(256);
		d_parent->d_results[i].clear();
		cv::Rect roi = partitions[i];
		toReturn.push_back([this,i,roi](const Frame::Ptr & frame,
		                                const cv::Mat & upstream,
		                                fort::FrameReadout & readout,
		                                cv::Mat & result) {
			                   cv::Mat withROI(frame->ToCV(),roi);
			                   cv::Mat cloned = withROI.clone();
			                   maytags::Detector::ListOfDetection detections;
			                   d_parent->d_detectors[i]->Detect(cloned,detections);
			                   Detection d;
			                   for( auto const & md : detections ) {
				                   d.ID = md.ID;
				                   d.X = md.Center.x() + roi.x;
				                   d.Y = md.Center.y() + roi.y;
				                   //d.Theta = ComputeAngleFromHomography(q);
				                   d.Theta = ComputeAngleFromCorner(md);
				                   d_parent->d_results[i].push_back(d);
			                   }

		                   });
	}
	return toReturn;
}


AprilTag2Detector::TagMerging::TagMerging(const AprilTag2Detector::Ptr & parent,
                                          const Connection::Ptr & connection)
	: d_parent(parent)
	, d_connection(connection) {
}

AprilTag2Detector::TagMerging::~TagMerging() {}

std::vector<ProcessFunction> AprilTag2Detector::TagMerging::Prepare(size_t maxProcess, const cv::Size & size) {
	return { [this](const Frame::Ptr & frame,
	                const cv::Mat & upstream,
	                fort::FrameReadout & readout,
	                cv::Mat & result) {
			std::set<int32_t> results;

			readout.set_timestamp(frame->Timestamp());
			readout.set_frameid(frame->ID());
			readout.clear_ants();
			auto time = readout.mutable_time();
			time->set_seconds(frame->Time().tv_sec);
			time->set_nanos(frame->Time().tv_usec * 1000);

			for(auto const & detections : d_parent->d_results ) {
				for( auto const & d : detections ) {
					if (results.count((int32_t)d.ID) != 0 ) {
						continue;
					}
					results.insert(d.ID);
					auto a = readout.add_ants();
					a->set_id(d.ID);
					a->set_x(d.X);
					a->set_y(d.Y);
					a->set_theta(d.Theta);
				}
			}
			if (d_connection) {
				Connection::PostMessage(d_connection,readout);
			}

			//clear any upstream transformation
			result = frame->ToCV();

		}
	};
}




ProcessQueue AprilTag2Detector::Create(size_t maxWorkers,
                                       const Config & config,
                                       const Connection::Ptr & connection) {
	auto detector = std::shared_ptr<AprilTag2Detector>(new AprilTag2Detector(maxWorkers,config));
	return {
		std::shared_ptr<ProcessDefinition>(new ROITagDetection(detector)),
		std::shared_ptr<ProcessDefinition>(new TagMerging(detector,connection)),
	};
}
