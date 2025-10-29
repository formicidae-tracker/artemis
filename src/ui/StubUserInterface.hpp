#pragma once

#include "UserInterface.hpp"

namespace fort {
namespace artemis {

class StubUserInterface : public UserInterface {
public:
	StubUserInterface(
	    const Size          &workingResolution,
	    const Options       &options,
	    const ROIChannelPtr &roiChannel
	);

	void Task() override;

	void UpdateFrame(const FrameToDisplay &frame, const DataToDisplay &data)
	    override;
};

} // namespace artemis
} // namespace fort
