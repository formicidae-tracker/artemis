#pragma once

#include "ProcessDefinition.h"

class WatermarkingProcess : public ProcessDefinition {
public :
	WatermarkingProcess(const std::string & watermark);
	virtual ~WatermarkingProcess();

	virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);
private :
	std::string d_text;
	cv::Size    d_textSize;
	int         d_baseline;
	int         d_fontface;
	int         d_thickness;
	double      d_fontscale;
};
