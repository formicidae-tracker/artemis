#pragma once

#include <functional>

#include <tbb/concurrent_queue.h>

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
		Time       FrameTime;
		double     FPS;
		size_t     FrameProcessed;
		size_t     FrameDropped;

	};

	typedef tbb::concurrent_queue<Zoom> ZoomChannel;
	typedef std::shared_ptr<ZoomChannel> ZoomChannelPtr;

	UserInterface(const cv::Size & workingResolution,
	              const DisplayOptions & options,
	              const ZoomChannelPtr & zoomChannel);


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
	ZoomChannelPtr     d_zoomChannel;
	std::set<uint32_t> d_highlighted;
	bool               d_displayROI;
};


} // namespace artemis
} // namespace fort
