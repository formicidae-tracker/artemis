#include "AcquisitionTask.hpp"

#include <artemis-config.h>
#include <memory>
#include <stdexcept>
#ifdef EURESYS_FRAMEGRABBER_SUPPORT
#include "EuresysFrameGrabber.hpp"
#endif // EURESYS_FRAMEGRABBER_SUPPORT

#ifdef HYPERION_FRAMEGRABBER_SUPPORT
#include "HyperionFrameGrabber.hpp"
#endif // HYPERION_FRAMEGRABBER_SUPPORT

#ifdef MULTICAM_FRAMEGRABBER_SUPPORT
#include "MULTICAMFrameGrabber.hpp"
#endif // MULTICAM_FRAMEGRABBER_SUPPORT

#include "StubFrameGrabber.hpp"

#include "ProcessFrameTask.hpp"

#include <glog/logging.h>

namespace fort {
namespace artemis {

FrameGrabber::Ptr AcquisitionTask::LoadFrameGrabber(
    const std::vector<std::string> &stubImagePaths, const CameraOptions &options
) {
#ifdef ARTEMIS_STUB_FRAMEGRABBER_ONLY
	return std::make_shared<StubFrameGrabber>(stubImagePaths, options.FPS);
#else
	if (stubImagePaths.empty() == false) {
		return std::make_shared<StubFrameGrabber>(stubImagePaths, options.FPS);
	}
#ifdef EURESYS_FRAMEGRABBER_SUPPORT
	static Euresys::EGenTL egentl;
	return std::make_shared<EuresysFrameGrabber>(egentl, options);
#endif // EURESYS_FRAMEGRABBER_SUPPORT

#ifdef HYPERION_FRAMEGRABBER_SUPPORT
	return std::make_shared<HyperionFrameGrabber>(0, options);
#endif

#ifdef MULTICAM_FRAMEGRABBER_SUPPORT
	throw std::runtime_error("Multicam support Not Yet Implemented");
#endif

#endif // ARTEMIS_FRAMEGRABBER_ONLY
}

AcquisitionTask::AcquisitionTask(
    const FrameGrabber::Ptr &grabber, const ProcessFrameTaskPtr &process
)
    : d_grabber(grabber)
    , d_processFrame(process) {
	d_quit.store(false);
}

AcquisitionTask::~AcquisitionTask() {}

void AcquisitionTask::Stop() {
	d_quit.store(true);
}

void AcquisitionTask::Run() {
	LOG(INFO) << "[AcquisitionTask]:  started";
	d_grabber->Start();
	while (d_quit.load() == false) {
		Frame::Ptr f = d_grabber->NextFrame();
		if (d_processFrame) {
			d_processFrame->QueueFrame(f);
		}
	}
	LOG(INFO) << "[AcquisitionTask]:  Tear Down";
	d_grabber->Stop();
	if (d_processFrame) {
		d_processFrame->CloseFrameQueue();
	}
	LOG(INFO) << "[AcquisitionTask]:  ended";
}

} // namespace artemis
} // namespace fort
