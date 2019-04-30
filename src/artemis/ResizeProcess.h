#pragma once

#include "ProcessDefinition.h"

#include <opencv2/core.hpp>



class ResizeProcess {
public:
	ResizeProcess(size_t height);

	virtual ~ResizeProcess();

	void operator()(const Frame::Ptr & frame, const cv::Mat & upstream, cv::Mat & result);

private:
	Frame::Ptr d_currentFrame;
	cv::Mat d_resized;
	size_t d_height;
	bool d_initialized;
};
