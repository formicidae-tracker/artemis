#pragma once

#include <functional>

#include "FrameGrabber.h"

#include <hermes/FrameReadout.pb.h>
#include <opencv2/core.hpp>


typedef std::function<void (const cv::Mat & partialFrame)> ParallelProcessFunction;

typedef std::function<void (const Frame::Ptr & frame, const  cv::Mat & upstream, cv::Mat & result)> PreTagFunction;

typedef std::function<void(const Frame::Ptr & frame,
                           const cv::Mat & upstream,
                           const fort::FrameReadout & readout,
                           cv::Mat & result)> PostTagFunction;


typedef std::vector<PreTagFunction > PreTagQueue;
typedef std::vector<PostTagFunction> PostTagQueue;
