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


void ImageResource::ExpectEqual(const cv::Mat & expected, const cv::Mat & actual) {
	ASSERT_EQ(expected.rows,actual.rows);
	ASSERT_EQ(expected.cols,actual.cols);
	ASSERT_EQ(expected.type(),actual.type());

	switch(expected.type()) {
	case CV_8U:
		ExpectEqual8uc1(expected,actual);
		break;
	case CV_8UC3:
		ExpectEqual8uc3(expected,actual);
		break;
	default:
		throw std::runtime_error("nsupported image type");
	}

}

void ImageResource::ExpectEqual8uc3(const cv::Mat & expected, const cv::Mat & actual) {
#ifndef NDEBUG
	cv::Mat diff(expected.size(),CV_8UC3);
#endif

	size_t nbErrors = 0;

	for(size_t y = 0; y < expected.rows; ++y) {
		for(size_t x = 0; x < expected.cols; ++x) {
			auto expectedP = expected.at<cv::Vec3b>(y,x);
			auto actualP = actual.at<cv::Vec3b>(y,x);

#ifndef NDEBUG
			if ( expectedP == actualP ) {
				diff.at<cv::Vec3b>(y,x) = expectedP;
			} else {
				diff.at<cv::Vec3b>(y,x) = cv::Vec3b(255-expectedP[0],255-expectedP[1],255-expectedP[2]);
				++nbErrors;
			}
#endif

			if (nbErrors > 20 ) {
				continue;
			}

			EXPECT_EQ(expectedP[0],actualP[0]);
			EXPECT_EQ(expectedP[1],actualP[1]);
			EXPECT_EQ(expectedP[2],actualP[2]);
		}
	}
#ifndef NDEBUG
	cv::imshow("expected",expected);
	cv::imshow("actual",actual);

	cv::imshow("difference",diff);

	cv::waitKey(0);

#endif

}

void ImageResource::ExpectEqual8uc1(const cv::Mat & expected,const cv::Mat & actual) {
#ifndef NDEBUG
	cv::Mat diff(expected.size(),CV_8UC3);
#endif

	size_t nbErrors = 0;

	for(size_t j = 0; j < expected.rows; ++j) {
		for(size_t i = 0; i < actual.cols; ++i) {
			auto expectedP = expected.at<uint8_t>(j,i);
			auto actualP = actual.at<uint8_t>(j,i);


			if (expectedP != actualP && nbErrors < 20 ) {
				++nbErrors;
				ADD_FAILURE() << "At (" << i << "," << j << ")";

			}

#ifndef NDEBUG
			cv::Vec3b color;
			if ( expectedP == actualP ) {
				color[2] = expectedP;
				color[1] = expectedP;
				color[0] = expectedP;
			} else if (expectedP > actualP) {
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
	cv::imshow("expected",expected);
	cv::imshow("actual",actual);

	cv::imshow("difference",diff);

	cv::waitKey(0);
#endif

}
