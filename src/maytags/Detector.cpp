#include "Detector.h"

#include "Implementation.h"

using namespace maytags;


Detector::QuadThresholdConfig::QuadThresholdConfig()
	: MinimumPixelPerCluster(5)
	, MaximumNumberOfMaxima(10)
	, CriticalCornerAngleRadian(10*M_PI / 180)
	, CosCriticalRadian(std::cos(CriticalCornerAngleRadian))
	, MaxLineMSE(10)
	, MinimumBWDifference(5) {
}

Detector::Config::Config()
	: DecodeSharpening(0.25) {
}

Detector::Detector(const Config & config)
	: d_implementation(new Implementation(config)) {
}

Detector::~Detector() {
}

void Detector::Detect(const cv::Mat & grayscale, ListOfDetection & detections) {
	d_implementation->Detect(grayscale,detections);
}
