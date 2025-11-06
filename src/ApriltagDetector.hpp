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

	static FamilyPtr createFamily(tags::Family family);
	static DetectorPtr
	createDetector(const ApriltagOptions &options, apriltag_family_t *family);

	static double computeAngleFromCorner(const apriltag_detection_t *q);

	static std::tuple<uint32_t, double, double, double>
	convertDetection(const apriltag_detection_t *q, const Rect &roi);

	void mergeDetection(
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

		Buffer(const Buffer &other) = delete;

		Buffer(Buffer &&other)
		    : data{other.data}
		    , size{other.size} {
			other.data = nullptr;
			other.size = 0;
		}

		Buffer &operator=(const Buffer &other) = delete;

		Buffer &operator=(Buffer &&other) {
			data       = other.data;
			size       = other.size;
			other.data = nullptr;
			other.size = 0;
			return *this;
		}

		~Buffer() {
			if (data != nullptr) {
				free(data);
			}
		}

		Buffer()
		    : data{nullptr}
		    , size{0} {}

		Buffer(size_t size)
		    : data{static_cast<uint8_t *>(aligned_alloc(64, (size + 63) & ~63))}
		    , size{size} {}
	};

	void setUpTaskflow();
	void cloneAndDetectPartition(size_t i);

	ImageU8               d_input;
	hermes::FrameReadout *d_readout = nullptr;

	Buffer d_buffer;

	size_t                  d_task_size = 0;
	Partition               d_current_partition;
	std::vector<ImageU8>    d_images;
	std::vector<zarray_t *> d_detections;

	double                d_minimumDetectionDistanceSquared;
	std::atomic<uint32_t> d_maximumConcurrency;
	tf::Taskflow          d_taskflow;
};

} // namespace artemis
} // namespace fort
