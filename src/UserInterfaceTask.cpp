#include "UserInterfaceTask.hpp"

#include "ui/GLUserInterface.hpp"

#include "glog/logging.h"

namespace fort {
namespace artemis {

UserInterfaceTask::UserInterfaceTask(
    const cv::Size &workingResolution,
    const cv::Size &fullResolution,
    const Options  &options
)
    : d_workingResolution(workingResolution)
    , d_fullResolution(fullResolution)
    , d_config(options)
    , d_defaultROI(cv::Point(0, 0), fullResolution)
    , d_roiChannel(std::make_shared<UserInterface::ROIChannel>()) {
	// UI not initialized here to ensure that it is initialized from
	// the working thread (i.e. once the task is spawned in Run())
}

UserInterfaceTask::~UserInterfaceTask() {

}

void UserInterfaceTask::Run() {
	LOG(INFO) << "[UserInterfaceTask]: Initialize OpenGL";

	d_ui = std::make_unique<GLUserInterface>(
	    d_workingResolution,
	    d_fullResolution,
	    d_config,
	    d_roiChannel
	);

	LOG(INFO) << "[UserInterfaceTask]: Started";

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
	LOG(INFO) << "[UserInterfaceTask]: Ended";
}

const cv::Rect &UserInterfaceTask::DefaultROI() const {
	return d_defaultROI;
}

cv::Rect UserInterfaceTask::UpdateROI(const cv::Rect &previous) {
	cv::Rect previous_ = previous;
	while (d_roiChannel->try_dequeue(previous_) == true) {
	}
	return previous_;
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
