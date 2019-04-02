#pragma once

#include "ProcessDefinition.h"


class DrawDetectionProcess : public ProcessDefinition {
public :
	virtual ~DrawDetectionProcess() {}

	virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);
private :
	static void DrawAnts(size_t start, size_t stride, const fort::FrameReadout & readout,cv::Mat & result);
	static void DrawAnt(const fort::Ant & a, cv::Mat & result, size_t size);
};
