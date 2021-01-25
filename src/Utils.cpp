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

cv::Mat GetROICenteredAtBlackBorder(const cv::Mat & image,
                                    const cv::Point & position,
                                    const cv::Size & size) {

	cv::Mat res(size,image.type());
	res.setTo(0);
	auto absolutePosition = cv::Point(position.x - size.width/2,position.y - size.height/2);
	auto imagePos = cv::Point(std::clamp(absolutePosition.x,0,image.cols - size.width),
	                          std::clamp(absolutePosition.y,0,image.rows - size.height));
	auto resPos = cv::Point(std::max(-absolutePosition.x,0),
	                        std::max(-absolutePosition.y,0));
	auto overlappingSize = size - cv::Size(resPos.x,resPos.y);
	overlappingSize -= cv::Size(std::clamp(absolutePosition.x-image.cols+size.width,0,size.width),
	                            std::clamp(absolutePosition.y-image.rows+size.height,0,size.height));

	if ( overlappingSize.width == 0 && overlappingSize.height == 0 ) {
		return res;
	}
	image(cv::Rect(imagePos,overlappingSize)).copyTo(res(cv::Rect(resPos,overlappingSize)));
	return res;
}


}
}
