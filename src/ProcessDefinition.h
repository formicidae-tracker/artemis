#pragma once

#include <functional>

#include "FrameGrabber.h"

#include <hermes/FrameReadout.pb.h>
#include <opencv2/core.hpp>


typedef std::function<void(const Frame::Ptr & frame,
                           const fort::FrameReadout & readout,
                           const cv::Mat & upstream)> ProcessFunction;

class ProcessDefinition {
public:
	virtual ~ProcessDefinition();

	virtual std::vector<ProcessFunction> Prepare(size_t nbProcess, const cv::Size &) =0;
	virtual void Finalize(const cv::Mat & upstream, fort::FrameReadout & readout, cv::Mat & result) = 0;
};
