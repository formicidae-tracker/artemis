#pragma once

#include <opencv2/core.hpp>

class ImageResource {
public:
	ImageResource(const uint8_t *start, const uint8_t * end);

	const cv::Mat & Image();
	static void ExpectEqual(const cv::Mat & expected,const cv::Mat & actual);
private :
	cv::Mat d_image;

	static void ExpectEqual8uc1(const cv::Mat & expected,const cv::Mat & actual);
	static void ExpectEqual8uc3(const cv::Mat & expected,const cv::Mat & actual);
};

#define LOAD_IMAGE(x) [](){	  \
		extern const uint8_t _binary_ ## x ## _start, _binary_ ## x ## _end; \
		return ImageResource(&_binary_ ## x ## _start, & _binary_ ## x ## _end); \
	}()
