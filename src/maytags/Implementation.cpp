#include "Implementation.h"

using namespace maytags;

Implementation::Implementation(const Detector::Config & config)
	: d_config(config) {
	if ( d_config.Families.empty() ) {
		throw std::invalid_argument("maytags::Detector::Config need at least one family");
	}
}

Implementation::~Implementation() {
}

void Implementation::Detect(const cv::Mat & grayscale, Detector::ListOfDetection & detections) {
	if (grayscale.channels() != CV_8U ) {
		throw std::invalid_argument("Only working with 8 bit monochrome images");
	}

	detections.clear();



}
