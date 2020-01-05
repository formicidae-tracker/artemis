#pragma once

#include "ProcessDefinition.h"


class DrawDetectionProcess : public ProcessDefinition {
public :
	DrawDetectionProcess();
	virtual ~DrawDetectionProcess();

	virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);

	void AddHighlighted(uint32_t tagID);
	void RemoveHighlighted(uint32_t tagID);
	void ToggleHighlighted(uint32_t tagID);


private :

	void DrawAnts(size_t start, size_t stride, const fort::hermes::FrameReadout & readout,cv::Mat & result,double ratio);
	void DrawAnt(const fort::hermes::Tag & a, cv::Mat & result, double ratio);

	inline bool IsHighlighted(uint32_t tagID) {
		return d_highlighted.count(tagID) != 0;
	}

	std::set<uint32_t> d_highlighted;

};
