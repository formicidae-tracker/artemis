#pragma once

#include "ProcessDefinition.h"

class ResizeProcess : public ProcessDefinition {
public:
	ResizeProcess(size_t width, size_t height);
	virtual ~ResizeProcess();


	virtual bool WantParallel() const;
	virtual ProcessFunction Definition();
	virtual void OnAllProcessEnd();

	const cv::Mat & Result();

private:
	cv::Mat d_result;
};
