#include <iostream>

#include <common/InterprocessBuffer.h>
#include <apriltag/apriltag.h>
#include <apriltag/tag36h11.h>

#include "../utils/PosixCall.h"
#include <glog/logging.h>
#include <Eigen/Core>

double ComputeAngleFromCorner(apriltag_detection *q) {
	Eigen::Vector2d c0(q->p[0][0],q->p[0][1]);
	Eigen::Vector2d c1(q->p[1][0],q->p[1][1]);
	Eigen::Vector2d c2(q->p[2][0],q->p[2][1]);
	Eigen::Vector2d c3(q->p[3][0],q->p[3][1]);

	Eigen::Vector2d delta = (c1 + c2) / 2.0 - (c0 + c3) / 2.0;

	return atan2(delta.y(),delta.x());
}

typedef std::shared_ptr<apriltag_family_t> FamilyPtr;

FamilyPtr CreateFamily(DetectionConfig::FamilyType t) {
	static std::vector<std::function<FamilyPtr()> > creators = {
		[]() { return FamilyPtr(tag36h11_create(),tag36h11_destroy); },
	};
	if ( (size_t)t >= creators.size() ) {
		throw std::out_of_range("Unknown FamilyType");
	}
	return creators[(size_t)t]();
}


void Execute(int argc, char **argv) {
	LOG(INFO) << "Orion started with ID '" << argv[1] << "'" ;
	if ( argc != 2 ) {
		throw std::invalid_argument("Missing shared memory segment");
	}

	InterprocessManager::Ptr manager = std::make_shared<InterprocessManager>();

	InterprocessBuffer ipBuffer(manager,std::atoi(argv[1]));

	auto family = CreateFamily(ipBuffer.Config().Family);
	std::shared_ptr<apriltag_detector_t> detector(apriltag_detector_create(),apriltag_detector_destroy);

	detector->nthreads = 1;
	detector->quad_decimate = 1;
	detector->quad_sigma = 0.0;
	detector->decode_sharpening = 0.25;
	detector->qtp.min_white_black_diff = ipBuffer.Config().QuadMinBWDiff;
	detector->qtp.max_line_fit_mse = ipBuffer.Config().QuadMaxLineMSE;
	detector->qtp.deglitch = 0;

	detector->qtp.critical_rad = ipBuffer.Config().QuadCriticalRadian;
	detector->qtp.cos_critical_rad = std::cos(ipBuffer.Config().QuadCriticalRadian);
	detector->qtp.max_nmaxima = ipBuffer.Config().QuadMaxNMaxima;
	detector->qtp.min_cluster_pixels = ipBuffer.Config().QuadMinClusterPixel;
	apriltag_detector_add_family(detector.get(),family.get());


	LOG(INFO) << "Entering infinite loop";
	uint64_t lastTS = std::numeric_limits<uint64_t>::max();
	for(;;) {
		uint64_t ts = manager->WaitForNewJob(lastTS);
		if (ts != ipBuffer.TimestampIn() ) {
			LOG(ERROR) << "Skipping frame: expected ts: " << ts << " got: " << ipBuffer.TimestampIn();
			lastTS = ts;
			continue;
		}
		image_u8_t img = {
			.width = ipBuffer.Image().cols,
			.height = ipBuffer.Image().rows,
			.stride = (int32_t) ipBuffer.Image().step[0],
			.buf = ipBuffer.Image().data
		};
		DLOG(INFO) << "Detecting image " << ipBuffer.TimestampIn();
		auto detections = apriltag_detector_detect(detector.get(),&img);
		apriltag_detection_t * q;
		DLOG(INFO) << zarray_size(detections)  << " detected";
		size_t size = std::min((size_t)zarray_size(detections),DETECTION_SIZE);
		ipBuffer.DetectionsSize() = size;
		for (size_t i = 0; i < size; ++i) {
			zarray_get(detections,i,&q);
			InterprocessBuffer::Detection * d = ipBuffer.Detections() + i;
			d->ID    = q->id;
			d->X     = q->c[0];
			d->Y     = q->c[1];
			d->Theta = ComputeAngleFromCorner(q);
		}
		zarray_destroy(detections);
		ipBuffer.TimestampOut() = ts;
		lastTS = ts;

		manager->PostJobFinished();
	}
}

void write_failure(const char* data, int size) {
	LOG(ERROR) << std::string(data,size);
}

int main(int argc, char ** argv) {
	::google::InitGoogleLogging(argv[0]);
	::google::InstallFailureSignalHandler();
	::google::InstallFailureWriter(write_failure);
	try {
		Execute(argc,argv);
	} catch (const std::exception & e) {
		LOG(ERROR) << "Unhandled error: " << e.what();
		return 1;
	}
	LOG(INFO) << "finished";
	return 0;
}
