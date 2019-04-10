#include "Apriltag2Process.h"

#include <apriltag/tag16h5.h>
#include <apriltag/tag25h9.h>
#include <apriltag/tag36h11.h>
#include <apriltag/tagCircle21h7.h>
#include <apriltag/tagCircle49h12.h>
#include <apriltag/tagCustom48h12.h>
#include <apriltag/tagStandard41h12.h>
#include <apriltag/tagStandard52h13.h>


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
	d_families.reserve(maxWorkers);
	for (size_t i = 0 ; i < maxWorkers; ++i ) {
		d_families.push_back(std::move(OpenFamily(config.Family)));
		DetectorPtr detector(apriltag_detector_create(),apriltag_detector_destroy);
		apriltag_detector_add_family(detector.get(),d_families[i].get());
		detector->nthreads = 1;

		detector->quad_decimate = config.QuadDecimate;
		detector->quad_sigma = config.QuadSigma;
		detector->refine_edges = config.RefineEdges ? 1 : 0;
		detector->debug = false;
		detector->qtp.min_cluster_pixels = config.QuadMinClusterPixel;
		detector->qtp.max_nmaxima = config.QuadMaxNMaxima;
		detector->qtp.critical_rad = config.QuadCriticalRadian;
		detector->qtp.max_line_fit_mse = config.QuadMaxLineMSE;
		detector->qtp.min_white_black_diff = config.QuadMinBWDiff;
		detector->qtp.deglitch = config.QuadDeglitch ? 1 : 0;

		d_detectors.push_back(std::move(detector));
	}
}

AprilTag2Detector::~AprilTag2Detector() {}


AprilTag2Detector::FamilyPtr AprilTag2Detector::OpenFamily(const std::string & name ) {
	struct FamilyInterface {
		typedef apriltag_family_t* (*Constructor) ();
		typedef void (*Destructor) (apriltag_family_t *);
		Constructor c;
		Destructor  d;
	};

	static std::map<std::string,FamilyInterface > familyFactory = {
		{"16h5",{.c = tag16h5_create, .d=tag16h5_destroy}},
		{"25h9",{.c =tag25h9_create, .d=tag25h9_destroy}},
		{"36h11",{.c =tag36h11_create, .d=tag36h11_destroy}},
		{"Circle21h7",{.c =tagCircle21h7_create, .d=tagCircle21h7_destroy}},
		{"Circle49h12",{.c =tagCircle49h12_create, .d=tagCircle49h12_destroy}},
		{"Custom48h12",{.c =tagCustom48h12_create, .d=tagCustom48h12_destroy}},
		{"Standard41h12",{.c =tagStandard41h12_create, .d=tagStandard41h12_destroy}},
		{"Standard52h13",{.c =tagStandard52h13_create, .d=tagStandard52h13_destroy}},
	};

	auto fi = familyFactory.find(name);
	if (fi == familyFactory.end() ) {
		throw std::runtime_error("Could not find tag family '"+ name +"'");
	}
	return FamilyPtr(fi->second.c(),fi->second.d);
}



AprilTag2Detector::ROITagDetection::ROITagDetection(const AprilTag2Detector::Ptr & parent)
	: d_parent(parent ) {
}

AprilTag2Detector::ROITagDetection::~ROITagDetection() {}

double ComputeAngleFromCorner(apriltag_detection *q) {
	Eigen::Vector2d c0(q->p[0][0],q->p[0][1]);
	Eigen::Vector2d c1(q->p[1][0],q->p[1][1]);
	Eigen::Vector2d c2(q->p[2][0],q->p[2][1]);
	Eigen::Vector2d c3(q->p[3][0],q->p[3][1]);

	Eigen::Vector2d delta = (c1 + c2) / 2.0 - (c0 + c3) / 2.0;


	return atan2(delta.y(),delta.x());
}

double ComputeAngleFromHomography(apriltag_detection *q) {
	typedef Eigen::Transform<double,2,Eigen::Projective> Homography;
	Homography hm;
	Eigen::Matrix2d rotation;
	Eigen::Matrix2d scaling;
	//angle estimation
	hm.matrix() <<
		q->H->data[0], q->H->data[1], q->H->data[2],
		q->H->data[3], q->H->data[4], q->H->data[5],
		q->H->data[6], q->H->data[7], q->H->data[8];

	hm.computeRotationScaling(&rotation,&scaling);
	return Eigen::Rotation2D<double>(rotation).angle();
}


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
		                                fort::FrameReadout & readout,		                                                                            cv::Mat & result) {
			                   cv::Mat withROI(frame->ToCV(),roi);
			                   image_u8_t img = {
				                   .width = withROI.cols,
				                   .height = withROI.rows,
				                   .stride = (int32_t) withROI.step[0],
				                   .buf = withROI.data
			                   };
			                   auto detections = apriltag_detector_detect(d_parent->d_detectors[i].get(),&img);
			                   apriltag_detection_t * q;
			                   Detection d;
			                   for( int j = 0; j < zarray_size(detections); ++j ) {
				                   zarray_get(detections,j,&q);
				                   d.ID = q->id;
				                   d.X = q->c[0] + roi.x;
				                   d.Y = q->c[1] + roi.y;
				                   //d.Theta = ComputeAngleFromHomography(q);
				                   d.Theta = ComputeAngleFromCorner(q);
				                   d_parent->d_results[i].push_back(d);
			                   }



			                   auto tp = d_parent->d_detectors[i]->tp;
			                   timeprofile_stamp(tp,"artemis/saving");
			                   int64_t lastutime = tp->utime;
			                   for(size_t j = 0; j < zarray_size(d_parent->d_detectors[i]->tp->stamps); ++j) {
				                   struct timeprofile_entry *stamp;
				                   zarray_get_volatile(tp->stamps,j,&stamp);
				                   double cumtimeMS = (stamp->utime - tp->utime)/1.0e3;
				                   double parttimeMS = (stamp->utime - lastutime)/1.0e3;
				                   lastutime = stamp->utime;
				                   DLOG(INFO) << i << "." << j
				                              <<": " << stamp->name
				                              << " itself:" << parttimeMS
				                              << "ms  total:" << cumtimeMS << "ms";
			                   }

			                   apriltag_detections_destroy(detections);
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
