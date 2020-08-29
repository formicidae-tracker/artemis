#include "Utils.hpp"


namespace fort {
namespace artemis {

cv::Rect GetROICenteredAt(const cv::Point & center,
                          const cv::Size & roiSize,
                          const cv::Size & bound) {
	int x = std::clamp(center.x - roiSize.width / 2,
	                   0,
	                   bound.width - roiSize.width);

	int y = std::clamp(center.y - roiSize.height / 2,
	                   0,
	                   bound.height - roiSize.height);

	return cv::Rect({x,y},roiSize);
}

}
}
