#pragma once

#include "Task.hpp"

#include "Options.hpp"

#include <fort/hermes/FrameReadout.pb.h>

#include <tbb/concurrent_queue.h>

#include "ui/UserInterface.hpp"

namespace fort {
namespace artemis {

class UserInterfaceTask : public Task {
public:

	UserInterfaceTask(const cv::Size & workingResolution,
	                  const DisplayOptions & options);

	virtual ~UserInterfaceTask();

	void Run() override;

	UserInterface::Zoom UnsafeCurrentZoom();

	UserInterface::Zoom UpdateZoom(const UserInterface::Zoom & previous);

	void QueueFrame(const UserInterface::FrameToDisplay &  toDisplay);

	void CloseQueue();

private:
	tbb::concurrent_queue<UserInterface::Zoom>           d_zoomQueue;
	tbb::concurrent_queue<UserInterface::FrameToDisplay> d_displayQueue;

	std::unique_ptr<UserInterface>        d_ui;
};

} // namespace artemis
} // namespace fort
