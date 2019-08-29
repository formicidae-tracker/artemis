#pragma once

#include "ProcessDefinition.h"

#include <opencv2/core/core.hpp>

class ResizeProcess : public ProcessDefinition {
public:
	ResizeProcess(size_t height, bool forceIntegerScaling);

	virtual ~ResizeProcess();

	virtual std::vector<ProcessFunction> Prepare(size_t nbProcess, const cv::Size & size);

private:
	Frame::Ptr d_currentFrame;
	cv::Mat d_resized;
	size_t d_height;
	bool d_initialized;
	bool d_forceIntegerScaling;
};
