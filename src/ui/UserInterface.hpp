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
	struct FrameToDisplay {
		std::shared_ptr<cv::Mat>              Full,Zoomed;
		std::shared_ptr<hermes::FrameReadout> Message;

		cv::Rect CurrentROI;

		//Other data
		Time       FrameTime;
		double     FPS;
		size_t     FrameProcessed;
		size_t     FrameDropped;
	};

	typedef tbb::concurrent_queue<cv::Rect> ROIChannel;
	typedef std::shared_ptr<ROIChannel> ROIChannelPtr;

	UserInterface(const cv::Size & workingResolution,
	              const Options & options,
	              const ROIChannelPtr & zoomChannel);


	virtual void PollEvents() = 0;

	void PushFrame(const FrameToDisplay & frame);

protected:
	struct DataToDisplay {
		std::vector<size_t> HighlightedIndexes,NormalIndexes;
	};

	virtual void UpdateFrame(const FrameToDisplay & frame,
	                         const DataToDisplay & data) = 0;

	void ROIChanged(const cv::Rect & roi);
	void ToggleHighlight(uint32_t tagID);
	void ToggleROIDisplay();
	void ToggleLabelDisplay();
	void ToggleDisplayHelp();
	void ToggleDisplayOverlay();

	std::string PromptAndValue() const;
	void EnterHighlightPrompt();
	void LeaveHighlightPrompt();
	void AppendPromptValue( char c );



	bool DisplayROI() const;
	bool DisplayLabels() const;
	bool DisplayHelp() const;
	bool DisplayOverlay() const;
	const std::string & Watermark() const;

	DataToDisplay ComputeDataToDisplay(const std::shared_ptr<hermes::FrameReadout> & m);

private:
	ROIChannelPtr      d_roiChannel;
	std::set<uint32_t> d_highlighted;
	const std::string  d_watermark;
	std::string        d_prompt,d_value;
	bool               d_displayROI;
	bool               d_displayLabels;
	bool               d_displayHelp;
	bool               d_displayOverlay;
};


} // namespace artemis
} // namespace fort
