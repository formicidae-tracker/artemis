#pragma once

#include "Task.hpp"

#include "FrameGrabber.hpp"
#include <tbb/concurrent_queue.h>

namespace fort {
namespace artemis {

class ProcessFrameTask : public Task{
public:
	virtual ~ProcessFrameTask();

	void Run() override;

	void QueueFrame( const Frame::Ptr & );
	void CloseFrameQueue();
private :
	typedef tbb::concurrent_bounded_queue<Frame::Ptr> FrameQueue;

	void ProcessFrameMandatory(const Frame::Ptr & frame );
	void ProcessFrame(const Frame::Ptr & frame);
	void DropFrame(const Frame::Ptr & frame);

	void TearDown();

	FrameQueue d_frameQueue;
};

} // namespace artemis
} // namespace fort
