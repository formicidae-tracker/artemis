#pragma once

#include <functional>

#include "FrameGrabber.h"

#include <fort-hermes/FrameReadout.pb.h>
#include <opencv2/core.hpp>


typedef std::function<void(const Frame::Ptr & frame,
                           const cv::Mat & upstream,
                           fort::hermes::FrameReadout & readout,
                           cv::Mat & result)> ProcessFunction;

class ProcessDefinition {
public:
	typedef std::shared_ptr<ProcessDefinition> Ptr;

	virtual ~ProcessDefinition();

	virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &) =0;
};


typedef std::vector<ProcessDefinition::Ptr> ProcessQueue;
