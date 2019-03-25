#pragma once

#include <functional>

#include "FrameGrabber.h"

#include <hermes/FrameReadout.pb.h>


class ProcessDefinition {
public:
	typedef size_t WID;
	typedef std::function<void(WID ID, size_t TotalProcess, const Frame::Ptr & ptr, fort::FrameReadout & frame)> ProcessFunction;

	virtual ~ProcessDefinition();

	virtual bool WantParallel() const = 0;
	virtual ProcessFunction Definition();
	virtual void OnAllProcessEnd() = 0;
};
