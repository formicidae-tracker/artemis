#pragma once

#include "UserInterface.hpp"

namespace fort {
namespace artemis {

class StubUserInterface : public UserInterface {
public:

	StubUserInterface(const cv::Size & workingResolution,
	                  const Options & options,
	                  const ROIChannelPtr & roiChannel);

	void PollEvents() override;

	void UpdateFrame(const FrameToDisplay & frame,
	                 const DataToDisplay & data) override;

};


} // namespace artemis
} // namespace fort
