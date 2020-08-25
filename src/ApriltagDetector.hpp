#pragma once

#include <apriltag/apriltag.h>
#include <fort/hermes/FrameReadout.pb.h>

#include "Options.hpp"
#include "utils/Partitions.hpp"

#include <functional>

namespace fort {
namespace artemis {



class ApriltagDetector {
public :
	ApriltagDetector(size_t maxParallel,
	                 const cv::Size & size,
	                 const ApriltagOptions & options);


	void Detect(const cv::Mat & mat,
	            size_t nThreads,
	            hermes::FrameReadout & m);


private:
	typedef std::unique_ptr<apriltag_family_t, std::function <void (apriltag_family_t *) > > FamilyPtr;
	typedef std::unique_ptr<apriltag_detector_t,std::function<void (apriltag_detector_t*)> > DetectorPtr;

	static FamilyPtr CreateFamily(tags::Family family);
	static DetectorPtr CreateDetector(const ApriltagOptions & options,
	                                  apriltag_family_t * family);

	static double ComputeAngleFromCorner(const apriltag_detection_t *q);

	std::tuple<uint32_t,double,double,double> ConvertDetection(const apriltag_detection_t * q,
	                                                           const cv::Rect & roi);

	std::vector<zarray_t*> PartionnedDetection(const cv::Mat & image,
	                                           const Partition & partition);

	void MergeDetection(const std::vector<zarray_t*> detections,
	                    const Partition & partition,
	                    hermes::FrameReadout & m);


	std::vector<FamilyPtr>   d_families;
	std::vector<DetectorPtr> d_detectors;
	std::vector<Partition>   d_partitions;

	double                   d_minimumDetectionDistanceSquared;
};


} // namespace artemis
} // namespace fort
