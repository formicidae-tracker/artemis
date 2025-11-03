#include "ApriltagDetector.hpp"
#include "Options.hpp"
#include "Rect.hpp"
#include "fort/options/details/Option.hpp"
#include "taskflow/core/runtime.hpp"
#include "taskflow/core/taskflow.hpp"
#include "utils/Partitions.hpp"

#include <apriltag/tag16h5.h>
#include <apriltag/tag25h9.h>
#include <apriltag/tag36h11.h>
#include <apriltag/tagCircle21h7.h>
#include <apriltag/tagCircle49h12.h>
#include <apriltag/tagCustom48h12.h>
#include <apriltag/tagStandard41h12.h>
#include <apriltag/tagStandard52h13.h>
#include <chrono>
#include <cstdint>
#include <fort/tags/fort-tags.hpp>
#include <fort/tags/tag36ARTag.h>
#include <fort/tags/tag36h10.h>

#include <Eigen/Core>
#include <Eigen/StdVector>
#include <memory>
#include <slog++/slog++.hpp>
#include <stdexcept>
#include <thread>

namespace fort {
namespace artemis {

// allocate a new 64 byte alligned image_u8_t image with 64 bytes alligned
// stride.
std::unique_ptr<image_u8_t, void (*)(image_u8_t *)>
allocNewImage(const Size &size) {
	slog::DDebug(
	    "aligned image allocation",
	    slog::Int("width", size.width()),
	    slog::Int("height", size.height())
	);

	int32_t  stride = (size.width() + 63) & ~63;
	uint8_t *buf =
	    static_cast<uint8_t *>(aligned_alloc(64, stride * size.height()));
	image_u8_t *img = new image_u8_t{
	    .width  = size.width(),
	    .height = size.height(),
	    .stride = stride,
	    .buf    = buf
	};
	return std::unique_ptr<image_u8_t, void (*)(image_u8_t *)>(
	    img,
	    [](image_u8_t *img) {
		    slog::DDebug("aligned image dealloc");
		    free(img->buf);
		    delete img;
	    }
	);
}

void image_u8_cpy(image_u8_t &dst, const image_u8_t &src) {
	if (dst.width != src.width || dst.height != src.height) {
		throw std::invalid_argument{"Size must match"};
	}
	if (dst.stride == src.stride) {
		memcpy(dst.buf, src.buf, src.height * src.stride * sizeof(uint8_t));
		return;
	}

	for (int32_t i = 0; i < src.height; ++i) {
		memcpy(
		    &dst.buf[i * dst.stride],
		    &src.buf[i * src.stride],
		    std::min(dst.stride, src.stride) * sizeof(uint8_t)
		);
	}
}

image_u8_t image_u8_get_ROI(const image_u8_t &src, const Rect &ROI) {
	if (src.width < (ROI.x() + ROI.width()) ||
	    src.height < (ROI.y() + ROI.height())) {
		throw std::invalid_argument{"ROI does not fit in src"};
	}

	return {
	    .width  = ROI.width(),
	    .height = ROI.height(),
	    .stride = src.stride,
	    .buf    = &src.buf[ROI.y() * src.stride + ROI.x()],
	};
}

image_u8_t image_u8_copy_ROI_from_buf(
    const image_u8_t &buffer, const image_u8_t &src, const Rect &ROI
) {
	image_u8_t res = {
	    .width  = ROI.width(),
	    .height = ROI.height(),
	    .stride = (ROI.width() + 63) & ~63,
	    .buf    = buffer.buf,
	};
	size_t needed    = res.stride * res.height;
	size_t available = src.stride * src.height;

	if (needed > available) {
		throw std::invalid_argument(
		    "Not enough space in buffer (needed:" + std::to_string(needed) +
		    " available: " + std::to_string(available) + ")"
		);
	}
	image_u8_cpy(res, image_u8_get_ROI(src, ROI));
	return res;
}

ApriltagDetector::ApriltagDetector(
    size_t maxParallel, const Size &size, const ApriltagOptions &options
)
    : d_family{CreateFamily(options.Family())}
    , d_maximumConcurrency{uint32_t(maxParallel)} {
	d_minimumDetectionDistanceSquared =
	    options.QuadMinClusterPixel * options.QuadMinClusterPixel;

	d_detectors.reserve(maxParallel);
	d_detectors.reserve(maxParallel);

	for (size_t i = 0; i < maxParallel; ++i) {

		d_detectors.push_back(std::move(CreateDetector(options, d_family.get()))
		);
		Partition partition;
		PartitionRectangle(Rect{{0, 0}, size}, i + 1, partition);
		AddMargin(size, 75, partition);

		// the first image is always the biggest one.
		d_images.emplace_back(allocNewImage(partition[i].Size()));
		d_partitions.push_back(partition);
	}

	d_taskflow.emplace([this](tf::Runtime &rt) {
		const size_t            task_size  = d_maximumConcurrency.load();
		const auto             &partitions = d_partitions[task_size - 1];
		std::vector<zarray_t *> detections{task_size, nullptr};

		for (size_t i = 0; i < task_size; ++i) {
			rt.silent_async([&partitions, i, this, &detections]() {
				try {
					auto img = image_u8_copy_ROI_from_buf(
					    *d_images[i],
					    *d_input,
					    partitions[i]
					);
					detections[i] =
					    apriltag_detector_detect(d_detectors[i].get(), &img);

				} catch (const std::exception &e) {
					slog::Error(
					    "apriltag error",
					    slog::Err(e),
					    slog::Int("i", i)
					);
				}
			});
		}
		rt.corun();
		d_readout.clear_tags();
		uint32_t quads{0};
		MergeDetection(detections, partitions, d_readout);
		for (size_t i = 0; i < task_size; ++i) {
			quads += d_detectors[i]->nquads;
		}

		d_readout.set_quads(quads);

		for (auto d : detections) {
			if (d != nullptr) {
				apriltag_detections_destroy(d);
			}
		}
	});
}

size_t ApriltagDetector::MaxConcurrency() const {
	return d_maximumConcurrency.load();
}

void ApriltagDetector::SetMaxConcurrency(size_t maxConcurrency) {
	d_maximumConcurrency.store(
	    std::clamp(maxConcurrency, size_t(1U), d_images.size())
	);
}

tf::Taskflow &ApriltagDetector::Taskflow() {
	return d_taskflow;
}

double ApriltagDetector::ComputeAngleFromCorner(const apriltag_detection_t *q) {

	Eigen::Vector2d c0(q->p[0][0], q->p[0][1]);
	Eigen::Vector2d c1(q->p[1][0], q->p[1][1]);
	Eigen::Vector2d c2(q->p[2][0], q->p[2][1]);
	Eigen::Vector2d c3(q->p[3][0], q->p[3][1]);

	Eigen::Vector2d delta = (c1 + c2) / 2.0 - (c0 + c3) / 2.0;

	return atan2(delta.y(), delta.x());
}

std::tuple<uint32_t, double, double, double> ApriltagDetector::ConvertDetection(
    const apriltag_detection_t *q, const Rect &roi
) {
	return {
	    q->id,
	    q->c[0] + roi.x(),
	    q->c[1] + roi.y(),
	    ComputeAngleFromCorner(q)
	};
}

void ApriltagDetector::MergeDetection(
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
		const auto &roi = partition[++i];
		for (int j = 0; j < zarray_size(localDetections); ++j) {
			zarray_get(localDetections, j, &q);
			const auto &[tagID, x, y, angle] = ConvertDetection(q, roi);
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

ApriltagDetector::FamilyPtr ApriltagDetector::CreateFamily(tags::Family family
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

ApriltagDetector::DetectorPtr ApriltagDetector::CreateDetector(
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

void ApriltagDetector::SetInput(const image_u8_t *image) {
	d_input = image;
}

const hermes::FrameReadout &ApriltagDetector::Readout() const {
	return d_readout;
}

} // namespace artemis
} // namespace fort
