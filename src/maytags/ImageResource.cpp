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


	switch(mat.type()) {
	case CV_8U:
		ExpectEqual8uc1(mat);
		break;
	case CV_8UC3:
		ExpectEqual8uc3(mat);
		break;
	default:
		throw std::runtime_error("nsupported image type");
	}

}

void ImageResource::ExpectEqual8uc3(const cv::Mat & mat) {
#ifndef NDEBUG
	cv::Mat diff(mat.size(),CV_8UC3);
#endif

	size_t nbErrors = 0;

	for(size_t y = 0; y < mat.rows; ++y) {
		for(size_t x = 0; x < mat.cols; ++x) {
			auto expected = d_image.at<cv::Vec3b>(y,x);
			auto actual = mat.at<cv::Vec3b>(y,x);

#ifndef NDEBUG
			if ( expected == actual ) {
				diff.at<cv::Vec3b>(y,x) = expected;
			} else {
				diff.at<cv::Vec3b>(y,x) = cv::Vec3b(255-expected[0],255-expected[1],255-expected[2]);
				++nbErrors;
			}
#endif

			if (nbErrors > 20 ) {
				continue;
			}

			EXPECT_EQ(expected[0],actual[0]);
			EXPECT_EQ(expected[1],actual[1]);
			EXPECT_EQ(expected[2],actual[2]);
		}
	}
#ifndef NDEBUG
	cv::imshow("expected",d_image);
	cv::imshow("actual",mat);

	cv::imshow("difference",diff);

	cv::waitKey(0);

#endif

}

void ImageResource::ExpectEqual8uc1(const cv::Mat & mat) {
#ifndef NDEBUG
	cv::Mat diff(mat.size(),CV_8UC3);
#endif

	size_t nbErrors = 0;

	for(size_t j = 0; j < mat.rows; ++j) {
		for(size_t i = 0; i < mat.cols; ++i) {
			auto expected = d_image.at<uint8_t>(j,i);
			auto actual = mat.at<uint8_t>(j,i);


			if (expected != actual && nbErrors < 20 ) {
				++nbErrors;
				ADD_FAILURE() << "At (" << i << "," << j << ")";

			}

#ifndef NDEBUG
			cv::Vec3b color;
			if ( expected == actual ) {
				color[2] = expected;
				color[1] = expected;
				color[0] = expected;
			} else if (expected > actual) {
				color[2] = 0xff;
				color[1] = 0;
				color[0] = 0;
			} else {
				color[2] = 0;
				color[1] = 0;
				color[0] = 0xff;
			}
			diff.at<cv::Vec3b>(j,i) = color;
#endif
		}
	}

#ifndef NDEBUG
	cv::imshow("expected",d_image);
	cv::imshow("actual",mat);

	cv::imshow("difference",diff);

	cv::waitKey(0);
#endif

}
