#include "UserInterfaceTask.hpp"

#include "ui/GLUserInterface.hpp"

#include "glog/logging.h"

namespace fort {
namespace artemis {



UserInterfaceTask::UserInterfaceTask(const cv::Size & workingResolution,
                                     const DisplayOptions & options)
	: d_workingResolution(workingResolution)
	, d_options(options)
	, d_defaultZoom({1.0,
	                 cv::Point(workingResolution.width/2,
	                           workingResolution.height/2)}) \
	, d_zoomChannel(std::make_shared<UserInterface::ZoomChannel>()) {
	// UI not initialized here to ensure that it is initialized from
	// the working thread (i.e. once the task is spawned in Run())
}

UserInterfaceTask::~UserInterfaceTask() {

}

void UserInterfaceTask::Run()  {
	LOG(INFO) << "[UserInterfaceTask]: Initialize OpenGL";

	d_ui = std::make_unique<GLUserInterface>(d_workingResolution,d_options,d_zoomChannel);

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
	d_ui.reset();
	LOG(INFO) << "[UserInterfaceTask]: Ended";
}


const UserInterface::Zoom & UserInterfaceTask::DefaultZoom() const {
	return d_defaultZoom;
}

UserInterface::Zoom UserInterfaceTask::UpdateZoom(const UserInterface::Zoom & previous) {
	UserInterface::Zoom previous_ = previous;
	while ( d_zoomChannel->try_pop(previous_) == true ) {
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
