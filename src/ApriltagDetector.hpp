#pragma once

#include <apriltag/apriltag.h>
#include <cstdint>
#include <cstdlib>
#include <fort/hermes/FrameReadout.pb.h>

#include <memory>
#include <taskflow/taskflow.hpp>

#include "ImageU8.hpp"
#include "Options.hpp"

#include "utils/Partitions.hpp"

#include "Rect.hpp"

namespace fort {
namespace artemis {

class ApriltagDetector {
public:
	ApriltagDetector(
	    size_t maxParallel, const Size &size, const ApriltagOptions &options
	);

	tf::Taskflow &Taskflow();

	size_t MaxConcurrency() const;
	void   SetMaxConcurrency(size_t maxConcurrency);
	void   SetInputOutput(const ImageU8 &image, hermes::FrameReadout *readout);

private:
	typedef std::unique_ptr<apriltag_family_t, void (*)(apriltag_family_t *)>
	    FamilyPtr;
	typedef std::
	    unique_ptr<apriltag_detector_t, void (*)(apriltag_detector_t *)>
	        DetectorPtr;

	static FamilyPtr CreateFamily(tags::Family family);
	static DetectorPtr
	CreateDetector(const ApriltagOptions &options, apriltag_family_t *family);

	static double ComputeAngleFromCorner(const apriltag_detection_t *q);

	std::tuple<uint32_t, double, double, double>
	ConvertDetection(const apriltag_detection_t *q, const Rect &roi);

	std::vector<zarray_t *>
	PartionnedDetection(image_u8_t *image, const Partition &partition);

	void MergeDetection(
	    const std::vector<zarray_t *> detections,
	    const Partition              &partition,
	    hermes::FrameReadout         &m
	);

	FamilyPtr                d_family;
	std::vector<DetectorPtr> d_detectors;
	std::vector<Partition>   d_partitions;

	struct Buffer {
		uint8_t *data;
		size_t   size;

		~Buffer() {
			if (data != nullptr) {
				free(const_cast<uint8_t *>(data));
			}
		}

		Buffer()
		    : data{nullptr}
		    , size{0} {}

		Buffer(size_t size)
		    : data{static_cast<uint8_t *>(aligned_alloc(64, size))}
		    , size{size} {}
	};

	void SetUpTaskflow();

	ImageU8               d_input;
	hermes::FrameReadout *d_readout = nullptr;

	Buffer                d_buffer;

	const size_t           f_task_size = 0;
	const Partition        f_partitions;
	std::vector<ImageU8>    f_images;
	std::vector<zarray_t *> f_detections;

	double                d_minimumDetectionDistanceSquared;
	std::atomic<uint32_t> d_maximumConcurrency;
	tf::Taskflow          d_taskflow;
};

} // namespace artemis
} // namespace fort
