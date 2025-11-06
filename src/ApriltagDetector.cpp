#include "ApriltagDetector.hpp"
#include "ImageU8.hpp"
#include "Options.hpp"
#include "Rect.hpp"
#include "taskflow/core/runtime.hpp"
#include "taskflow/core/taskflow.hpp"
#include "utils/Partitions.hpp"
#include "utils/Slog.hpp"

#include <apriltag/tag16h5.h>
#include <apriltag/tag25h9.h>
#include <apriltag/tag36h11.h>
#include <apriltag/tagCircle21h7.h>
#include <apriltag/tagCircle49h12.h>
#include <apriltag/tagCustom48h12.h>
#include <apriltag/tagStandard41h12.h>
#include <apriltag/tagStandard52h13.h>
#include <cstdint>
#include <cstdlib>
#include <fort/hermes/FrameReadout.pb.h>
#include <fort/tags/fort-tags.hpp>
#include <fort/tags/tag36ARTag.h>
#include <fort/tags/tag36h10.h>

#include <Eigen/Core>
#include <Eigen/StdVector>
#include <memory>
#include <slog++/slog++.hpp>
#include <stdexcept>

namespace fort {
namespace artemis {

ApriltagDetector::ApriltagDetector(
    size_t maxParallel, const Size &size, const ApriltagOptions &options
)
    : d_family{createFamily(options.Family())}
    , d_maximumConcurrency{uint32_t(maxParallel)} {
	d_minimumDetectionDistanceSquared =
	    options.QuadMinClusterPixel * options.QuadMinClusterPixel;

	d_detectors.reserve(maxParallel);
	d_detectors.reserve(maxParallel);

	size_t bufferSize{0};

	for (size_t i = 0; i < maxParallel; ++i) {

		d_detectors.push_back(std::move(createDetector(options, d_family.get()))
		);

		Partition partition;
		PartitionRectangle(Rect{{0, 0}, size}, i + 1, partition);
		AddMargin(size, 75, partition);

		// the first image is always the biggest one.
		d_partitions.push_back(partition);
		size_t partitionSize{0};
		for (auto p : partition) {
			ImageU8 asImg{p.width(), p.height(), nullptr};
			partitionSize += asImg.NeededSize();
			slog::DDebug(
			    "partition",
			    slog::Int("size", i),
			    slogRect("partition", p),
			    slog::Int("neededBytes", asImg.NeededSize())
			);
		}
		bufferSize = std::max(partitionSize, bufferSize);
		slog::DDebug(
		    "partition",
		    slog::Int("size", i),
		    slog::Int("totalBytes", partitionSize)
		);
	}
	d_buffer = Buffer{bufferSize};
	slog::Info(
	    "allocated buffer",
	    slog::String("task", "ApriltagDetection"),
	    slog::Float("size_MB", d_buffer.size / 1024.0f / 1024.0f),
		slog::Int("size", d_buffer.size),
		slog::Pointer("address",d_buffer.data)
	);

	d_images.resize(maxParallel);
	d_detections.resize(maxParallel, nullptr);

	setUpTaskflow();
}

void ApriltagDetector::setUpTaskflow() {

	auto allocatePartition =
	    d_taskflow
	        .emplace([this]() {
		        if (d_input.buffer == nullptr || d_readout == nullptr) {
			        throw std::runtime_error("No input data");
		        }

		        *const_cast<size_t *>(&d_task_size) =
		            d_maximumConcurrency.load();
		        *const_cast<Partition *>(&d_current_partition) =
		            d_partitions[d_task_size - 1];

		        size_t used = 0;
		        for (size_t i = 0; i < d_task_size; ++i) {
			        auto partition = d_current_partition[i];
			        d_images[i]    = ImageU8{
                        partition.width(),
                        partition.height(),
                        d_buffer.data + used
                    };

			        if ((used + d_images[i].NeededSize()) > d_buffer.size) {
				        throw std::runtime_error(
				            "not enough buffer size for partition:" +
				            std::to_string(i) + " needed: " +
				            std::to_string(d_images[i].NeededSize()) +
				            " available: " + std::to_string(d_buffer.size) +
				            " already used: " + std::to_string(used)
				        );
			        }

			        used += d_images[i].NeededSize();
			        slog::DTrace(
			            "Partition allocated",
			            slog::Int("i", i),
			            slog::Pointer("pointer", d_images[i].buffer),
			            slog::Int("size", d_images[i].NeededSize()),
			            slog::Int("available", d_buffer.size - used)
			        );
		        }
	        })
	        .name("allocatePartitionedROIs");

	auto merge =
	    d_taskflow
	        .emplace([this]() {
		        auto detections = std::vector<zarray_t *>{
		            d_detections.begin(),
		            d_detections.begin() + d_task_size
		        };
		        mergeDetection(detections, d_current_partition, *d_readout);
		        size_t quads{0};
		        for (size_t i = 0; i < d_task_size; ++i) {
			        quads += d_detectors[i]->nquads;
		        }

		        d_readout->set_quads(quads);

		        for (auto &d : detections) {
			        if (d != nullptr) {
				        apriltag_detections_destroy(d);
			        }
			        d = nullptr;
		        }
	        })
	        .name("merge");

	for (size_t i = 0; i < d_maximumConcurrency; ++i) {
		auto detect_i =
		    d_taskflow.emplace([this, i]() { cloneAndDetectPartition(i); }
		    ).name("cloneAndDetect[" + std::to_string(i) + "]");
		detect_i.succeed(allocatePartition);
		detect_i.precede(merge);
	}
}

void ApriltagDetector::cloneAndDetectPartition(size_t i) {
	if (i >= d_task_size) {
		return;
	}
	ImageU8::Copy(d_images[i], d_input.GetROI(d_current_partition[i]));
	image_u8_t img{
	    .width  = d_images[i].width,
	    .height = d_images[i].height,
	    .stride = d_images[i].stride,
	    .buf    = d_images[i].buffer,
	};
	d_detections[i] = apriltag_detector_detect(d_detectors[i].get(), &img);
}

size_t ApriltagDetector::MaxConcurrency() const {
	return d_maximumConcurrency.load();
}

void ApriltagDetector::SetMaxConcurrency(size_t maxConcurrency) {
	d_maximumConcurrency.store(
	    std::clamp(maxConcurrency, size_t(1U), d_partitions.back().size())
	);
}

tf::Taskflow &ApriltagDetector::Taskflow() {
	return d_taskflow;
}

double ApriltagDetector::computeAngleFromCorner(const apriltag_detection_t *q) {
	Eigen::Vector2d c0(q->p[0][0], q->p[0][1]);
	Eigen::Vector2d c1(q->p[1][0], q->p[1][1]);
	Eigen::Vector2d c2(q->p[2][0], q->p[2][1]);
	Eigen::Vector2d c3(q->p[3][0], q->p[3][1]);

	Eigen::Vector2d delta = (c1 + c2) / 2.0 - (c0 + c3) / 2.0;

	return atan2(delta.y(), delta.x());
}

std::tuple<uint32_t, double, double, double> ApriltagDetector::convertDetection(
    const apriltag_detection_t *q, const Rect &roi
) {
	return {
	    q->id,
	    q->c[0] + roi.x(),
	    q->c[1] + roi.y(),
	    computeAngleFromCorner(q)
	};
}

void ApriltagDetector::mergeDetection(
    const std::vector<zarray_t *> detections,
    const Partition              &partition,
    hermes::FrameReadout         &m
) {
	typedef std::
	    vector<Eigen::Vector2d, Eigen::aligned_allocator<Eigen::Vector2d>>
	        Vector2dList;

	typedef std::map<
	    uint32_t,
	    Vector2dList,
	    std::less<uint32_t>,
	    Eigen::aligned_allocator<std::pair<const uint32_t, Vector2dList>>>
	    MapOfPoints;

	MapOfPoints points;

	apriltag_detection_t *q;
	size_t                i = -1;
	for (const auto &localDetections : detections) {
		if (localDetections == nullptr) {
			continue;
		}
		const auto &roi = partition[++i];
		for (int j = 0; j < zarray_size(localDetections); ++j) {
			zarray_get(localDetections, j, &q);
			const auto &[tagID, x, y, angle] = convertDetection(q, roi);
			Eigen::Vector2d currentPoint{x, y};
			auto            fi = std::find_if(
                points[tagID].cbegin(),
                points[tagID].cend(),
                [this, &currentPoint](const Eigen::Vector2d &p) {
                    return (currentPoint - p).squaredNorm() <
                           d_minimumDetectionDistanceSquared;
                }
            );
			if (fi != points[tagID].cend()) {
				continue;
			}
			points[tagID].push_back(currentPoint);

			auto t = m.add_tags();
			t->set_id(tagID);
			t->set_x(x);
			t->set_y(y);
			t->set_theta(angle);
		}
	}
}

ApriltagDetector::FamilyPtr ApriltagDetector::createFamily(tags::Family family
) {
	typedef apriltag_family_t *(*FamilyConstructor)();
	typedef void (*FamilyDestructor)(apriltag_family_t *);

	static std::
	    map<tags::Family, std::tuple<FamilyConstructor, FamilyDestructor>>
	        families = {
	            {tags::Family::Tag16h5, {tag16h5_create, tag16h5_destroy}},
	            {tags::Family::Tag25h9, {tag25h9_create, tag25h9_destroy}},
	            {tags::Family::Tag36h10, {tag36h10_create, tag36h10_destroy}},
	            {tags::Family::Tag36h11, {tag36h11_create, tag36h11_destroy}},
	            {tags::Family::Tag36ARTag,
	             {tag36ARTag_create, tag36ARTag_destroy}},
	            {tags::Family::Circle21h7,
	             {tagCircle21h7_create, tagCircle21h7_destroy}},
	            {tags::Family::Circle49h12,
	             {tagCircle49h12_create, tagCircle49h12_destroy}},
	            {tags::Family::Custom48h12,
	             {tagCustom48h12_create, tagCustom48h12_destroy}},
	            {tags::Family::Standard41h12,
	             {tagStandard41h12_create, tagStandard41h12_destroy}},
	            {tags::Family::Standard52h13,
	             {tagStandard52h13_create, tagStandard52h13_destroy}},
	        };

	if (family == tags::Family::Undefined) {
		throw std::invalid_argument("Family is not defined");
	}

	if (families.count(family) == 0) {
		throw std::runtime_error(
		    "Unknown fort::tags::Family(" + std::to_string(int(family)) + ")"
		);
	}
	const auto &f = families.at(family);

	return {std::get<0>(f)(), std::get<1>(f)};
}

ApriltagDetector::DetectorPtr ApriltagDetector::createDetector(
    const ApriltagOptions &options, apriltag_family_t *family
) {
	auto d = apriltag_detector_create();
	apriltag_detector_add_family(d, family);
	d->nthreads                 = 1;
	d->quad_decimate            = options.QuadDecimate;
	d->quad_sigma               = options.QuadSigma;
	d->refine_edges             = options.RefineEdges ? 1 : 0;
	d->debug                    = false;
	d->qtp.min_cluster_pixels   = options.QuadMinClusterPixel;
	d->qtp.max_nmaxima          = options.QuadMaxNMaxima;
	d->qtp.critical_rad         = options.QuadCriticalRadian;
	d->qtp.max_line_fit_mse     = options.QuadMaxLineMSE;
	d->qtp.min_white_black_diff = options.QuadMinBWDiff;
	d->qtp.deglitch             = options.QuadDeglitch ? 1 : 0;

	return DetectorPtr(d, apriltag_detector_destroy);
}

void ApriltagDetector::SetInputOutput(
    const ImageU8 &image, hermes::FrameReadout *readout
) {
	d_input   = image;
	d_readout = readout;
}

} // namespace artemis
} // namespace fort
