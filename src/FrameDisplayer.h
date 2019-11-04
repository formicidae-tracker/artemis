#pragma once

#include "ProcessDefinition.h"

class DrawDetectionProcess;
class OverlayWriter;
class FrameDisplayer : public ProcessDefinition {
public:

	FrameDisplayer(bool desactivateQuit,
	               const std::shared_ptr<DrawDetectionProcess> & ddProcess,
	               const std::shared_ptr<OverlayWriter> & oWriter);
	virtual ~FrameDisplayer();

	virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);

private:
	const static double MinZoom;
	const static double MaxZoom;
	const static double ZoomIncrement;

	static void StaticOnMouseCallback(int event, int x , int y, int flags, void* myself);
	void OnMouseCallback(int event, int x, int y, int flags);



	cv::Rect BuildROI(const cv::Size & size);




	std::vector <uint8_t>          d_data;
	bool                           d_initialized;
	int                            d_x,d_y;
	int                            d_mouseLastX,d_mouseLastY;
	double                         d_zoom;
	bool                           d_inhibQuit;

	std::shared_ptr<DrawDetectionProcess> d_ddProcess;
	std::shared_ptr<OverlayWriter>        d_oWriter;

	int32_t                                d_highlight;
};
