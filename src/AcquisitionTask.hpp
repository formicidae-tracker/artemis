#pragma once

#include "Task.hpp"

#include "FrameGrabber.hpp"

#include "Options.hpp"

#include <atomic>

namespace fort {
namespace artemis {

class ProcessFrameTask;
typedef std::shared_ptr<ProcessFrameTask> ProcessFrameTaskPtr;


class AcquisitionTask : public Task {
public:
	static FrameGrabber::Ptr LoadFrameGrabber(const std::vector<std::string> & stubImagePaths,
	                                          const CameraOptions & options);

	AcquisitionTask(const FrameGrabber::Ptr & grabber,
	                const ProcessFrameTaskPtr &  process);

	virtual ~AcquisitionTask();

	void Run() override;

	void Stop();

private:

	FrameGrabber::Ptr   d_grabber;
	ProcessFrameTaskPtr d_processFrame;
	std::atomic<bool>   d_quit;
};

} // namespace artemis
} // namespace fort
