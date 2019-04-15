#pragma once

#include <opencv2/core.hpp>

namespace maytags {

class Thresholder {
public:

	void Threshold(const cv::Mat & image,uint8_t minBWDiff);

	cv::Mat Thresholded;
private :
	const static size_t TileSize = 4;

	cv::Mat d_min,d_max,d_minTmp,d_maxTmp;

};

}
