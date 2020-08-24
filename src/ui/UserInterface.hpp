#pragma once

#include <opencv2/core.hpp>
#include <fort/hermes/FrameReadout.pb.h>

#include "../Options.hpp"

namespace fort {
namespace artemis {

class UserInterface {
public:

	struct Zoom {
		double    Scale;
		cv::Point Center;
	};


	struct FrameToDisplay {
		std::shared_ptr<cv::Mat>              Full,Zoomed;
		std::shared_ptr<hermes::FrameReadout> Message;

		Zoom CurrentZoom;

		//Other data
		double     FPS;
		size_t     FrameProcessed;
		size_t     FrameDropped;

	};

	typedef std::function<void(Zoom)> OnZoomCallback;

	UserInterface(const cv::Size & workingResolution,
	              const DisplayOptions & options);

	void OnZoom(const OnZoomCallback & callback);

	Zoom CurrentZoom() const;

	virtual void PollEvents() = 0;

	void PushFrame(const FrameToDisplay & frame);

protected:
	struct DataToDisplay {
		std::vector<size_t> HighlightedIndexes,NormalIndexes;
		bool                DisplayROI;
	};

	virtual void UpdateFrame(const FrameToDisplay & frame,
	                         const DataToDisplay & data) = 0;

	void ZoomChanged(const Zoom & zoom);
	void ToggleHighlight(uint32_t tagID);
	void SetROIDisplay(bool displayROI);

	DataToDisplay ComputeDataToDisplay(const std::shared_ptr<hermes::FrameReadout> & m);

private:
	Zoom               d_zoom;
	OnZoomCallback     d_onZoom;
	std::set<uint32_t> d_highlighted;
	bool               d_displayROI;
};


} // namespace artemis
} // namespace fort
