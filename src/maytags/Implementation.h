#pragma once

#include "Detector.h"


namespace maytags {
class Implementation {
public :
	Implementation(const Detector::Config & config);
	~Implementation();

	void Detect(const cv::Mat & grayscale, Detector::ListOfDetection & detections);

private :
	Detector::Config d_config;


};


}
