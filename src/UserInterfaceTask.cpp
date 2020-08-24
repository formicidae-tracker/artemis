#include "UserInterfaceTask.hpp"

#include "ui/StubUserInterface.hpp"

#include "glog/logging.h"

namespace fort {
namespace artemis {


UserInterfaceTask::UserInterfaceTask(const cv::Size & workingResolution,
                                     const DisplayOptions & options) {

	d_ui = std::make_unique<StubUserInterface>(workingResolution,options);

	d_ui->OnZoom([this](const UserInterface::Zoom & zoom) {
		             d_zoomQueue.push(zoom);
	             });

}

UserInterfaceTask::~UserInterfaceTask() {

}

void UserInterfaceTask::Run()  {
	LOG(INFO) << "[UserInterfaceTask]: Started";
	UserInterface::FrameToDisplay frame;
	for (;;) {
		d_ui->PollEvents();
		bool hasNew(false);
		while(d_displayQueue.try_pop(frame) == true) {
			hasNew = true;
		}
		if ( hasNew == false ) {
			continue;
		}
		if ( !frame.Full) {
			break;
		}
		d_ui->PushFrame(frame);
	}
	LOG(INFO) << "[UserInterfaceTask]: Ended";
}


UserInterface::Zoom UserInterfaceTask::UnsafeCurrentZoom() {
	return d_ui->CurrentZoom();
}

UserInterface::Zoom UserInterfaceTask::UpdateZoom(const UserInterface::Zoom & previous) {
	UserInterface::Zoom previous_ = previous;
	while ( d_zoomQueue.try_pop(previous_) == true ) {
	}
	return previous_;
}

void UserInterfaceTask::QueueFrame(const UserInterface::FrameToDisplay &  toDisplay) {
	d_displayQueue.push(toDisplay);
}

void UserInterfaceTask::CloseQueue() {
	d_displayQueue.push({.Full = nullptr });
}



} // namespace artemis

} // namespace fort
