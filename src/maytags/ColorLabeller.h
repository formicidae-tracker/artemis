#pragma once

#include <opencv2/core.hpp>

#include <map>

namespace maytags {

class ColorLabeller {
public:

	ColorLabeller();

	cv::Vec3b Color(uint64_t value);

private :
	const static std::vector<cv::Vec3b> s_colors;

	size_t d_nextColor;
	std::map<uint32_t,size_t> d_colorMapping;
};

}
