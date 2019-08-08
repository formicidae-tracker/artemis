#pragma once

#include "ProcessDefinition.h"

class FrameDisplayer : public ProcessDefinition {
public:

   	FrameDisplayer();
	virtual ~FrameDisplayer();

	virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);

private:


	std::vector <uint8_t>          d_data;
};
