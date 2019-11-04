#pragma once

#include "ProcessDefinition.h"


class DrawDetectionProcess : public ProcessDefinition {
public :
	DrawDetectionProcess();
	virtual ~DrawDetectionProcess();

	virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);
private :

	static void DrawAnts(size_t start, size_t stride, const fort::hermes::FrameReadout & readout,cv::Mat & result,double ratio);
	static void DrawAnt(const fort::hermes::Tag & a, cv::Mat & result, double ratio);

};
