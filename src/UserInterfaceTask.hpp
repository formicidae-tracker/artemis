#pragma once

#include "Task.hpp"

#include "Options.hpp"

#include <fort/hermes/FrameReadout.pb.h>

#include "readerwriterqueue.h"
#include "ui/GLUserInterface.hpp"
#include "ui/UserInterface.hpp"

namespace fort {
namespace artemis {

class UserInterfaceTask : public Task {
public:
	UserInterfaceTask(
	    const cv::Size &workingResolution,
	    const cv::Size &fullResolution,
	    const Options  &options
	);

	virtual ~UserInterfaceTask();

	void Run() override;

	const cv::Rect &DefaultROI() const;

	cv::Rect UpdateROI(const cv::Rect &previous);

	void QueueFrame(const UserInterface::FrameToDisplay &toDisplay);

	void CloseQueue();

private:
	moodycamel::ReaderWriterQueue<UserInterface::FrameToDisplay> d_displayQueue;

	std::unique_ptr<UserInterface> d_ui;
	const cv::Size                 d_workingResolution, d_fullResolution;
	UserInterface::Options         d_config;
	const cv::Rect                 d_defaultROI;
	UserInterface::ROIChannelPtr   d_roiChannel;
};

} // namespace artemis
} // namespace fort
