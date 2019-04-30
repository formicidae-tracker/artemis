#pragma once

#include "ProcessDefinition.h"


class DrawDetectionProcess {
public :
	virtual ~DrawDetectionProcess() {}

	void operator() (const Frame::Ptr & frame,
	                 const cv::Mat & upstream,
	                 const fort::FrameReadout & readout,
	                 cv::Mat & result);

private :
	static void DrawAnts(size_t start, size_t stride, const fort::FrameReadout & readout,cv::Mat & result,double ratio);
	static void DrawAnt(const fort::Ant & a, cv::Mat & result, int size, double ratio);
};
