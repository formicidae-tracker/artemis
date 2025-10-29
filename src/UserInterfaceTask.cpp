#include "UserInterfaceTask.hpp"

#include "ui/GLUserInterface.hpp"

namespace fort {
namespace artemis {

UserInterfaceTask::UserInterfaceTask(
    const Size    &workingResolution,
    const Size    &fullResolution,
    const Options &options
)
    : d_workingResolution(workingResolution)
    , d_fullResolution(fullResolution)
    , d_config(options)
    , d_defaultROI({0, 0}, fullResolution)
    , d_roiChannel(std::make_shared<UserInterface::ROIChannel>())
    , d_logger{slog::With(slog::String("task", "UserInterface"))} {
	// UI not initialized here to ensure that it is initialized from
	// the working thread (i.e. once the task is spawned in Run())
}

UserInterfaceTask::~UserInterfaceTask() {}

void UserInterfaceTask::Run() {
	d_logger.Info("Initializing OpenGL");

	d_ui = std::make_unique<GLUserInterface>(
	    d_workingResolution,
	    d_fullResolution,
	    d_config,
	    d_roiChannel
	);
	d_logger.Info("started");

	UserInterface::FrameToDisplay frame;
	for (;;) {
		d_ui->Task();
		bool hasNew(false);
		while (d_displayQueue.try_dequeue(frame) == true) {
			hasNew = true;
		}
		if (hasNew == false) {
			continue;
		}
		if (!frame.Full) {
			break;
		}
		d_ui->PushFrame(frame);
	}
	d_ui.reset();
	d_logger.Info("ended");
}

const Rect &UserInterfaceTask::DefaultROI() const {
	return d_defaultROI;
}

Rect UserInterfaceTask::UpdateROI(const Rect &previous) {
	Rect previous_ = previous;
	while (d_roiChannel->try_dequeue(previous_) == true) {
	}
	return previous;
}

void UserInterfaceTask::QueueFrame(
    const UserInterface::FrameToDisplay &toDisplay
) {
	d_displayQueue.enqueue(toDisplay);
}

void UserInterfaceTask::CloseQueue() {
	UserInterface::FrameToDisplay end;
	end.Full      = nullptr;
	end.FrameTime = fort::Time();

	d_displayQueue.enqueue(end);
}

} // namespace artemis

} // namespace fort
