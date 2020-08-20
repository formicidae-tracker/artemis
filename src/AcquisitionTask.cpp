#include "AcquisitionTask.hpp"

#include <artemis-config.h>
#ifndef FORCE_STUB_FRAMEGRABBER_ONLY
#include "EuresysFrameGrabber.hpp"
#endif //FORCE_STUB_FRAMEGRABBER_ONLY
#include "StubFrameGrabber.hpp"

#include "ProcessFrameTask.hpp"


namespace fort {
namespace artemis {

FrameGrabber::Ptr AcquisitionTask::LoadFrameGrabber(const std::string & stubImagePath,
                                                    const CameraOptions & options) {
#ifndef FORCE_STUB_FRAMEGRABBER_ONLY
	if (stubPath.empty() ) {
		return std::make_shared<EuresysFrameGrabber>(options);
	} else {
		return std::make_shared<StubFrameGrabber>(stubImagePath);
	}
#else
	return std::make_shared<StubFrameGrabber>(stubImagePath);
#endif
}


AcquisitionTask::AcquisitionTask(const FrameGrabber::Ptr & grabber,
                                 const ProcessFrameTaskPtr &  process)
	: d_grabber(grabber)
	, d_processFrame(process) {
	d_quit.store(false);
}

AcquisitionTask::~AcquisitionTask() { }

void AcquisitionTask::Stop() {
	d_quit.store(true);
}

void AcquisitionTask::Run() {
	d_grabber->Start();
	while(d_quit.load() == false ) {
		Frame::Ptr f = d_grabber->NextFrame();
		if ( d_processFrame ) {
			d_processFrame->QueueFrame(f);
		}
	}
	d_grabber->Stop();
	if (d_processFrame) {
		d_processFrame->CloseFrameQueue();
	}
}


} // namespace artemis
} // namespace fort
