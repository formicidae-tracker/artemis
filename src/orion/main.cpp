#include <iostream>

#include <common/InterprocessBuffer.h>
#include <maytags/Detector.h>
#include "../utils/PosixCall.h"
#include <glog/logging.h>

double ComputeAngleFromCorner(const maytags::Detection & d) {
	Eigen::Vector2d delta = (d.Corners[0] + d.Corners[2]) / 2.0 - (d.Corners[1] + d.Corners[3]) / 2.0;

	return atan2(delta.y(),delta.x());
}


void Execute(int argc, char **argv) {
	LOG(INFO) << "Orion started with ID '" << argv[1] << "'" ;
	if ( argc != 2 ) {
		throw std::invalid_argument("Missing shared memory segment");
	}

	InterprocessManager::Ptr manager = std::make_shared<InterprocessManager>();

	InterprocessBuffer ipBuffer(manager,std::atoi(argv[1]));

	maytags::Detector::Config config;
	config.DecodeSharpening = 0.25;
	config.QuadConfig.MinimumBWDifference = ipBuffer.Config().QuadMinBWDiff;
	config.QuadConfig.MaxLineMSE = ipBuffer.Config().QuadMaxLineMSE;
	config.QuadConfig.CriticalCornerAngleRadian = ipBuffer.Config().QuadCriticalRadian;
	config.QuadConfig.CosCriticalRadian = std::cos(ipBuffer.Config().QuadCriticalRadian);
	config.QuadConfig.MaximumNumberOfMaxima = ipBuffer.Config().QuadMaxNMaxima;
	config.QuadConfig.MinimumPixelPerCluster = ipBuffer.Config().QuadMinClusterPixel;
	config.Families.push_back(maytags::Family::Create(DetectionConfig::FamilyName(ipBuffer.Config().Family)));

	maytags::Detector detector(config);
	LOG(INFO) << "Entering infinite loop";
	for(;;) {
		manager->WaitForNewJob();

		maytags::Detector::ListOfDetection detections;
		detector.Detect(ipBuffer.Image(),detections);

		size_t size = std::min(detections.size(),DETECTION_SIZE);
		ipBuffer.DetectionsSize() = size;
		for (size_t i = 0; i < size; ++i) {
			InterprocessBuffer::Detection * d = ipBuffer.Detections() + i;
			d->ID    = detections[i].ID;
			d->X     = detections[i].Center.x();
			d->Y     = detections[i].Center.y();
			d->Theta = ComputeAngleFromCorner(detections[i]);
		}
		ipBuffer.TimestampOut() = ipBuffer.TimestampIn();

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
