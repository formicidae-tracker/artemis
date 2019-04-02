#include "Apriltag2Process.h"

#include <apriltag/tag16h5.h>
#include <apriltag/tag25h7.h>
#include <apriltag/tag25h9.h>
#include <apriltag/tag36artoolkit.h>
#include <apriltag/tag36h10.h>
#include <apriltag/tag36h11.h>

#include <cmath>

#include <google/protobuf/util/delimited_message_util.h>

#include <asio/write.hpp>
#include <asio/connect.hpp>
#include <asio/posix/stream_descriptor.hpp>


#include <glog/logging.h>
#include <opencv2/imgcodecs.hpp>


#include "utils/PosixCall.h"





AprilTag2Detector::AprilTag2Detector(const AprilTag2Detector::Config & config)
	: d_family(OpenFamily(config.Family))
	, d_detector(apriltag_detector_create(),apriltag_detector_destroy) {
	apriltag_detector_add_family(d_detector.get(),d_family.get());
	d_detector->nthreads = 1;

	d_detector->quad_decimate = config.QuadDecimate;
	d_detector->quad_sigma = config.QuadSigma;
	d_detector->refine_edges = config.RefineEdges ? 1 : 0;
	d_detector->refine_decode = config.NoRefineDecode ? 0 : 1;
	d_detector->refine_pose = config.RefinePose ? 1 : 0;
	d_detector->debug = false;
	d_detector->qtp.min_cluster_pixels = config.QuadMinClusterPixel;
	d_detector->qtp.max_nmaxima = config.QuadMaxNMaxima;
	d_detector->qtp.critical_rad = config.QuadCriticalRadian;
	d_detector->qtp.max_line_fit_mse = config.QuadMaxLineMSE;
	d_detector->qtp.min_white_black_diff = config.QuadMinBWDiff;
	d_detector->qtp.deglitch = config.QuadDeglitch ? 1 : 0;

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
		{"25h7",{.c =tag25h7_create, .d=tag25h7_destroy}},
		{"25h9",{.c =tag25h9_create, .d=tag25h9_destroy}},
		{"36artoolkit",{.c =tag36artoolkit_create, .d=tag36artoolkit_destroy}},
		{"36h10",{.c =tag36h10_create, .d=tag36h10_destroy}},
		{"36h11",{.c =tag36h11_create, .d=tag36h11_destroy}}
	};

	auto fi = familyFactory.find(name);
	if (fi == familyFactory.end() ) {
		throw std::runtime_error("Could not find tag family '"+ name +"'");
	}
	return FamilyPtr(fi->second.c(),fi->second.d);
}



cv::Rect computeROI(size_t i, size_t nbROI, const cv::Size & imageSize, size_t margin) {
	size_t bandWidth = imageSize.width/nbROI;
	size_t leftMargin = (i == 0 ) ? 0 : margin;
	size_t rightMargin = (i == (nbROI - 1) ) ? 0 : margin;
	cv::Point2d roiOrig ((i*bandWidth) - leftMargin ,0);
	cv::Size roiSize(bandWidth + leftMargin + rightMargin, imageSize.height);
	return cv::Rect(roiOrig,roiSize);
}

AprilTag2Detector::ROITagDetection::ROITagDetection(const AprilTag2Detector::Ptr & parent)
	: d_parent(parent ) {
}

AprilTag2Detector::ROITagDetection::~ROITagDetection() {}

std::vector<ProcessFunction> AprilTag2Detector::ROITagDetection::Prepare(size_t maxProcess, const cv::Size & size ) {
	d_parent->d_results.resize(maxProcess);
	d_parent->d_offsets.resize(maxProcess);

	std::vector<ProcessFunction> toReturn;
	toReturn.reserve(maxProcess);
	for (size_t i = 0; i < maxProcess; ++i) {
		d_parent->d_results[i] = NULL;
		size_t margin = 75;
		cv::Rect roi = computeROI(i,maxProcess,size,margin);
		d_parent->d_offsets[i] = roi.x;

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

			                   d_parent->d_results[i] = apriltag_detector_detect(d_parent->d_detector.get(),&img);
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
			std::map<int32_t,apriltag_detection_t*> results;

			for(size_t i = 0; i < d_parent->d_results.size(); ++i ) {
				for( int j = 0; j < zarray_size(d_parent->d_results[i]); ++j ) {
					apriltag_detection_t * q;
					zarray_get(d_parent->d_results[i],j,&q);
					if (results.count((int32_t)q->id) != 0 ) {
						continue;
					}
					results[q->id] = q;
					q->c[0] += d_parent->d_offsets[i];
				}
			}
			readout.set_timestamp(frame->Timestamp());
			readout.set_frameid(frame->ID());
			readout.clear_ants();
			auto time = readout.mutable_time();
			time->set_seconds(frame->Time().tv_sec);
			time->set_nanos(frame->Time().tv_usec * 1000);
			for ( auto & kv : results ) {
				auto a = readout.add_ants();
				a->set_id(kv.second->id);
				a->set_x(kv.second->c[0]);
				a->set_y(kv.second->c[1]);
				//TODO: compute angle!!!
				a->set_theta(0.0);
			}

			for(size_t i = 0 ; i < d_parent->d_results.size(); ++ i ) {
				apriltag_detections_destroy(d_parent->d_results[i]);
			}

			if (d_connection) {
				Connection::PostMessage(d_connection,readout);
			}


			//clear any upstream transformation
			result = frame->ToCV();


		}
	};
}




ProcessQueue AprilTag2Detector::Create(const Config & config,
                                       const Connection::Ptr & connection) {
	auto detector = std::shared_ptr<AprilTag2Detector>(new AprilTag2Detector(config));
	return {
		std::shared_ptr<ProcessDefinition>(new ROITagDetection(detector)),
			std::shared_ptr<ProcessDefinition>(new TagMerging(detector,connection)),
	};
}
