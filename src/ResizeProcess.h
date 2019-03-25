#pragma once

#include "ProcessDefinition.h"

class ResizeProcess : public ProcessDefinition {
public:
	ResizeProcess(size_t width, size_t height);


	virtual ~ResizeProcess();

	virtual std::vector<ProcessFunction> Prepare(size_t nbProcess);
	virtual void Finalize(const cv::Mat & upstream, fort::FrameReadout & readout, cv::Mat & result);

private:
	Frame::Ptr d_currentFrame;
	cv::Mat d_resized;
};
