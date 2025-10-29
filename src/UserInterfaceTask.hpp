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
	    const Size    &workingResolution,
	    const Size    &fullResolution,
	    const Options &options
	);

	virtual ~UserInterfaceTask();

	void Run() override;

	const Rect &DefaultROI() const;

	Rect UpdateROI(const Rect &previous);

	void QueueFrame(const UserInterface::FrameToDisplay &toDisplay);

	void CloseQueue();

private:
	moodycamel::ReaderWriterQueue<UserInterface::FrameToDisplay> d_displayQueue;

	std::unique_ptr<UserInterface> d_ui;
	const Size                     d_workingResolution, d_fullResolution;
	UserInterface::Options         d_config;
	const Rect                     d_defaultROI;
	UserInterface::ROIChannelPtr   d_roiChannel;
	slog::Logger<1>                d_logger;
};

} // namespace artemis
} // namespace fort
