#include "Implementation.h"

using namespace maytags;

Implementation::Implementation(const Detector::Config & config)
	: d_config(config) {
}

Implementation::~Implementation() {
}

void Implementation::Detect(const cv::Mat & grayscale, Detector::ListOfDetection & detections) {
	detections.clear();


}
