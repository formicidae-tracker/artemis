#pragma once

#include "ProcessDefinition.h"


class OverlayWriter : public ProcessDefinition {
public :
	OverlayWriter();
	virtual ~OverlayWriter();

	virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);
private :

	typedef uint8_t fontchar[16];

	static fontchar fontdata[256];

	static void LoadFontData();

	static void DrawFrameNumber(const fort::hermes::FrameReadout & readout,cv::Mat & result);

	static void DrawText(cv::Mat & img, const std::string & text, size_t x, size_t y);

};
