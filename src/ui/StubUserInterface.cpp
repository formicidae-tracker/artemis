#include "StubUserInterface.hpp"

#include <glog/logging.h>


namespace fort {
namespace artemis {

StubUserInterface::StubUserInterface(const cv::Size & workingResolution,
                                     const Options & options,
                                     const ROIChannelPtr & roiChannel)
	: UserInterface(workingResolution,options,roiChannel) {
}

void StubUserInterface::PollEvents() {
}

void StubUserInterface::UpdateFrame(const FrameToDisplay & frame,
                                    const DataToDisplay &) {
	LOG(INFO) << "Displaying a new frame. Processed: " << frame.FrameProcessed
	          << " Dropped: " << frame.FrameDropped
	          << " FPS: " << frame.FPS << " nquad: " << frame.Message->quads();
}


} // namespace artemis
} // namespace fort
