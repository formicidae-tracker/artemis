#include "ColorLabeller.h"

using namespace maytags;

const std::vector<cv::Vec3b> ColorLabeller::s_colors = {
	cv::Vec3b(219,112,147),
	cv::Vec3b(200,0,0),
	cv::Vec3b(230,100,60),
	cv::Vec3b(242,155,120),
	cv::Vec3b(230,224,176),
	cv::Vec3b(170,178,32),
	cv::Vec3b(50,205,154),
	cv::Vec3b(87,139,46),
	cv::Vec3b(190,230,245),
	cv::Vec3b(135,184,222),
	cv::Vec3b(0,225,255),
	cv::Vec3b(0,165,255),
	cv::Vec3b(0,69,255),
	cv::Vec3b(34,34,178),
	cv::Vec3b(193,182,255),
	cv::Vec3b(147,20,255),
};


ColorLabeller::ColorLabeller()
	: d_nextColor(0) {
}

cv::Vec3b ColorLabeller::Color(uint64_t value) {
	if (d_colorMapping.count(value) == 0) {
		d_colorMapping[value] = d_nextColor;
		d_nextColor = (d_nextColor + 1) % s_colors.size();
	}

	return s_colors[d_colorMapping[value]];
}
