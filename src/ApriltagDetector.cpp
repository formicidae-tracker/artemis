#include "ApriltagDetector.hpp"

#include <fort/tags/fort-tags.h>

#include <tbb/parallel_for.h>

#include <Eigen/Core>
#include <Eigen/StdVector>

namespace fort {
namespace artemis {

ApriltagDetector::ApriltagDetector(size_t maxParallel,
                                   const cv::Size & size,
                                   const ApriltagOptions & options) {
	d_minimumDetectionDistanceSquared = options.QuadMinClusterPixel * options.QuadMinClusterPixel;

	d_detectors.reserve(maxParallel);
	d_detectors.reserve(maxParallel);

	for ( size_t i = 0 ; i < maxParallel; ++ i ) {
		d_families.push_back(std::move(CreateFamily(options.Family)));
		d_detectors.push_back(std::move(CreateDetector(options,d_families.back().get())));
		Partition partition;
		PartitionRectangle(::cv::Rect(::cv::Point(0,0),size),
		                   i + 1,
		                   partition);
		AddMargin(size,75,partition);
		d_partitions.push_back(partition);
	}
}

void ApriltagDetector::Detect(const cv::Mat & image,
                              size_t nThreads,
                              hermes::FrameReadout & m) {

	nThreads = std::min(std::max(nThreads,size_t(1)),d_detectors.size());

	const auto & partition = d_partitions[nThreads-1];
	auto detections = PartionnedDetection(image,partition);
	MergeDetection(detections,partition,m);
	size_t nQuads = 0;
	for ( size_t i = 0; i < nThreads; ++i ) {
		nQuads += d_detectors[i]->nquads;
	}
	m.set_quads(nQuads);
	for ( auto & d : detections ) {
		apriltag_detections_destroy(d);
	}
}



std::vector<zarray_t*> ApriltagDetector::PartionnedDetection(const cv::Mat & image,
                                                             const Partition & partition) {

	std::vector<zarray_t*> detections(partition.size(),nullptr);
	tbb::parallel_for(tbb::blocked_range<size_t>(0,partition.size()),
	                  [&] ( const tbb::blocked_range<size_t> &  range ) {
		                  for ( size_t i = range.begin();
		                        i != range.end();
		                        ++i ) {
			                  auto cloned = cv::Mat(image,partition[i]).clone();
			                  image_u8 img = { .width = cloned.cols,
			                                   .height = cloned.rows,
			                                   .stride = cloned.cols,
			                                   .buf = cloned.data };
			                  detections[i] = apriltag_detector_detect(d_detectors[i].get(),&img);
		                  }
	                  });
	return detections;

}

double ApriltagDetector::ComputeAngleFromCorner(const apriltag_detection_t *q) {

	Eigen::Vector2d c0(q->p[0][0],q->p[0][1]);
	Eigen::Vector2d c1(q->p[1][0],q->p[1][1]);
	Eigen::Vector2d c2(q->p[2][0],q->p[2][1]);
	Eigen::Vector2d c3(q->p[3][0],q->p[3][1]);

	Eigen::Vector2d delta = (c1 + c2) / 2.0 - (c0 + c3) / 2.0;


	return atan2(delta.y(),delta.x());
}


std::tuple<uint32_t,double,double,double>
ApriltagDetector::ConvertDetection(const apriltag_detection_t * q,
                                   const cv::Rect & roi) {
	return {q->id,
	        q->c[0] + roi.x,
	        q->c[1] + roi.y,
	        ComputeAngleFromCorner(q)};
}


void ApriltagDetector::MergeDetection(const std::vector<zarray_t*> detections,
                                      const Partition & partition,
                                      hermes::FrameReadout & m) {
	typedef std::vector<Eigen::Vector2d,Eigen::aligned_allocator<Eigen::Vector2d> > Vector2dList;

	typedef std::map<uint32_t,Vector2dList,std::less<uint32_t>,
	                 Eigen::aligned_allocator<std::pair<const uint32_t,Vector2dList>>> MapOfPoints;

	MapOfPoints points;

	apriltag_detection_t * q;
	size_t i = -1;
	for ( const auto & localDetections : detections ) {
		const auto & roi = partition[++i];
		for ( int j = 0; j < zarray_size(localDetections); ++j) {
			zarray_get(localDetections,j,&q);
			const auto & [tagID,x,y,angle] = ConvertDetection(q,roi);
			Eigen::Vector2d currentPoint(x,y);
			auto fi = std::find_if(points[tagID].cbegin(),
			                       points[tagID].cend(),
			                       [this,&currentPoint]( const Eigen::Vector2d & p) {
				                       return (currentPoint -p).squaredNorm() < d_minimumDetectionDistanceSquared;
			                       });
			if ( fi != points[tagID].cend() ) {
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


ApriltagDetector::FamilyPtr ApriltagDetector::CreateFamily(tags::Family family) {
	typedef std::function<apriltag_family_t*()> FamilyConstructor;
	typedef std::function<void (apriltag_family_t*)> FamilyDestructor;

	static std::map<tags::Family,std::pair<FamilyConstructor,FamilyDestructor>> families
		= {
		   {tags::Family::Tag16h5,{tag16h5_create,tag16h5_destroy}},
		   {tags::Family::Tag25h9,{tag25h9_create,tag25h9_destroy}},
		   {tags::Family::Tag36h10,{tag36h10_create,tag36h10_destroy}},
		   {tags::Family::Tag36h11,{tag36h11_create,tag36h11_destroy}},
		   {tags::Family::Tag36ARTag,{tag36ARTag_create,tag36ARTag_destroy}},
		   {tags::Family::Circle21h7,{tagCircle21h7_create,tagCircle21h7_destroy}},
		   {tags::Family::Circle49h12,{tagCircle49h12_create,tagCircle49h12_destroy}},
		   {tags::Family::Custom48h12,{tagCustom48h12_create,tagCustom48h12_destroy}},
		   {tags::Family::Standard41h12,{tagStandard41h12_create,tagStandard41h12_destroy}},
		   {tags::Family::Standard52h13,{tagStandard52h13_create,tagStandard52h13_destroy}},
	};

	if ( family == tags::Family::Undefined ) {
		throw std::invalid_argument("Family is not defined");
	}

	auto fi = families.find(family);
	if ( fi == families.cend() ) {
		throw std::runtime_error("Unknown fort::tags::Family(" + std::to_string(int(family)) + ")");
	}

	return FamilyPtr(fi->second.first(),fi->second.second);
}

ApriltagDetector::DetectorPtr ApriltagDetector::CreateDetector(const ApriltagOptions & options,
                                                               apriltag_family_t * family) {
	auto d = apriltag_detector_create();
	apriltag_detector_add_family(d,family);
	d->nthreads = 1;
	d->quad_decimate = options.QuadDecimate;
	d->quad_sigma = options.QuadSigma;
	d->refine_edges = options.RefineEdges ? 1 : 0;
	d->debug = false;
	d->qtp.min_cluster_pixels = options.QuadMinClusterPixel;
	d->qtp.max_nmaxima = options.QuadMaxNMaxima;
	d->qtp.critical_rad = options.QuadCriticalRadian;
	d->qtp.max_line_fit_mse = options.QuadMaxLineMSE;
	d->qtp.min_white_black_diff = options.QuadMinBWDiff;
	d->qtp.deglitch = options.QuadDeglitch ? 1 : 0;

	return DetectorPtr(d,apriltag_detector_destroy);
}

} // namespace artemis
} // namespace fort
