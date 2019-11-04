#pragma once

#include "ProcessDefinition.h"


class DrawDetectionProcess : public ProcessDefinition {
public :
	DrawDetectionProcess();
	virtual ~DrawDetectionProcess();

	virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);

	const static uint32_t NO_HIGHLIGHT = -1;

	void SetHighlighted(uint32_t tagID);

private :

	static void DrawAnts(size_t start, size_t stride, const fort::hermes::FrameReadout & readout,cv::Mat & result,double ratio, uint32_t highlighted);
	static void DrawAnt(const fort::hermes::Tag & a, cv::Mat & result, double ratio, uint32_t highlighted);

	uint32_t d_highlighted;

};
