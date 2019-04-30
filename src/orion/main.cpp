#include <iostream>

#include <common/InterprocessBuffer.h>
#include <maytags/Detector.h>
#include "../utils/PosixCall.h"

double ComputeAngleFromCorner(const maytags::Detection & d) {
	Eigen::Vector2d delta = (d.Corners[0] + d.Corners[2]) / 2.0 - (d.Corners[1] + d.Corners[3]) / 2.0;

	return atan2(delta.y(),delta.x());
}


void Execute(int argc, char **argv) {
	if ( argc != 1 ) {
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
	config.Families.push_back(maytags::Family::Create(ipBuffer.Config().Family));

	maytags::Detector detector(config);

	for(;;) {
		uint8_t ts;
		int readen = read(STDIN_FILENO,&ts,sizeof(uint8_t));
		if ( readen < 1 ) {
			throw ARTEMIS_SYSTEM_ERROR(read,errno);
		}
		if (readen == 0) {
			continue;
		}
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

		int written = write(STDOUT_FILENO,&ts,sizeof(uint8_t));
		if ( written < 1) {
			throw ARTEMIS_SYSTEM_ERROR(read,errno);
		}
	}
}

int main(int argc, char ** argv) {
	try {
		Execute(argc,argv);
	} catch (const std::exception & e) {
		std::cerr << "Unhandled error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
