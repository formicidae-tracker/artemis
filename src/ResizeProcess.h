#pragma once

#include "ProcessDefinition.h"

#include <opencv2/core.hpp>

class ResizeProcess : public ProcessDefinition {
public:
	ResizeProcess(size_t width, size_t height);


	virtual ~ResizeProcess();

	virtual std::vector<ProcessFunction> Prepare(size_t nbProcess, const cv::Size & size);
	virtual void Finalize(const cv::Mat & upstream, fort::FrameReadout & readout, cv::Mat & result);

private:
	Frame::Ptr d_currentFrame;
	cv::Mat d_resized;
};
