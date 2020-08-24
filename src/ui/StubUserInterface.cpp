#include "StubUserInterface.hpp"

namespace fort {
namespace artemis {

StubUserInterface::StubUserInterface(const cv::Size & workingResolution,
                                     const DisplayOptions & options)
	: UserInterface(workingResolution,options) {
}

void StubUserInterface::PollEvents() {
}

void StubUserInterface::UpdateFrame(const FrameToDisplay &,
                                    const DataToDisplay &) {

}


} // namespace artemis
} // namespace fort
