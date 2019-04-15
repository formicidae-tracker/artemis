#include "ImageResource.h"

#include <opencv2/imgcodecs.hpp>
#include <vector>

#include <gtest/gtest.h>


#ifndef NDEBUG
#include <opencv2/highgui.hpp>
#endif


ImageResource::ImageResource(const uint8_t *start, const uint8_t * end) {
	std::vector<uint8_t> data;
	data.reserve(end-start);
	for ( ; start < end; ++start) {
		data.push_back(*start);
	}
	cv::imdecode(data,-1,&d_image);
	if (d_image.data == NULL ) {
		throw std::runtime_error("Could not load image");
	}
}

const cv::Mat & ImageResource::Image() {
	return d_image;
}

void ImageResource::ExpectEqual(const cv::Mat & mat) {
	ASSERT_EQ(mat.rows,d_image.rows);
	ASSERT_EQ(mat.cols,d_image.cols);

#ifndef NDEBUG
	cv::Mat diff(mat.size(),CV_8UC3);
#endif

	for(size_t j = 0; j < mat.rows; ++j) {
		for(size_t i = 0; i < mat.cols; ++i) {
			auto expected = d_image.at<uint8_t>(j,i);
			auto actual = mat.at<uint8_t>(j,i);

			EXPECT_EQ(expected,actual) << "At (" << i << "," << j << ")";
#ifndef NDEBUG
			cv::Vec3b color;
			color[2] = expected;
			if ( expected == actual ) {
				color[1] = expected;
				color[0] = expected;
			} else {
				color[1] = 0;
				color[0] = 0;
			}
			diff.at<cv::Vec3b>(j,i) = color;
#endif
		}
	}

#ifndef NDEBUG
	cv::imshow("difference",diff);

	cv::waitKey(0);
#endif

}
