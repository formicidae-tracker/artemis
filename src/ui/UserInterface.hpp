#pragma once

#include <functional>

#include "../Options.hpp"
#include "../Rect.hpp"

#include "ImageU8.hpp"
#include "readerwriterqueue.h"

#include <fort/hermes/FrameReadout.pb.h>

namespace fort {
namespace artemis {

class UserInterface {
public:
	struct Options {
		std::set<uint32_t> Highlighted;
		bool               TestMode;
		size_t             NewAntROISize;

		Options(const fort::artemis::Options &options)
		    : Highlighted{options.Display.Highlighted()}
		    , TestMode{options.TestMode}
		    , NewAntROISize{options.NewAntROISize} {}
	};

	struct FrameToDisplay {
		std::shared_ptr<ImageU8>              Full, Zoomed;
		std::shared_ptr<hermes::FrameReadout> Message;

		Rect CurrentROI;

		// Other data
		Time   FrameTime;
		double FPS;
		size_t FrameProcessed;
		size_t FrameDropped;
		size_t VideoOutputProcessed;
		size_t VideoOutputDropped;
	};

	typedef moodycamel::ReaderWriterQueue<Rect> ROIChannel;
	typedef std::shared_ptr<ROIChannel>         ROIChannelPtr;

	UserInterface(
	    const Size                   &workingResolution,
	    const UserInterface::Options &options,
	    const ROIChannelPtr          &zoomChannel
	);

	virtual void Task() = 0;

	void PushFrame(const FrameToDisplay &frame);

protected:
	struct DataToDisplay {
		std::vector<size_t> HighlightedIndexes, NormalIndexes;
	};

	virtual void
	UpdateFrame(const FrameToDisplay &frame, const DataToDisplay &data) = 0;

	void ROIChanged(const Rect &roi);
	void ToggleHighlight(uint32_t tagID);
	void ToggleDisplayROI();
	void ToggleDisplayLabels();
	void ToggleDisplayHelp();
	void ToggleDisplayOverlay();

	std::string        PromptAndValue() const;
	void               EnterHighlightPrompt();
	void               LeaveHighlightPrompt();
	void               AppendPromptValue(char c);
	void               ErasePromptLastValue();
	const std::string &Value() const;

	bool               DisplayROI() const;
	bool               DisplayLabels() const;
	bool               DisplayHelp() const;
	bool               DisplayOverlay() const;
	const std::string &Watermark() const;

	DataToDisplay
	ComputeDataToDisplay(const std::shared_ptr<hermes::FrameReadout> &m);

private:
	ROIChannelPtr      d_roiChannel;
	std::set<uint32_t> d_highlighted;
	const std::string  d_watermark;
	std::string        d_prompt, d_value;
	bool               d_displayROI;
	bool               d_displayLabels;
	bool               d_displayHelp;
	bool               d_displayOverlay;
};

} // namespace artemis
} // namespace fort
