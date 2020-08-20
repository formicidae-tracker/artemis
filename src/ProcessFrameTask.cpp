#include "ProcessFrameTask.hpp"

namespace fort {
namespace artemis {

ProcessFrameTask::~ProcessFrameTask() {}

void ProcessFrameTask::Run() {

	Frame::Ptr frame;
	for (;;) {
		d_frameQueue.pop(frame);
		if ( !frame ) {
			break;
		}

		ProcessFrameMandatory(frame);

		if ( d_frameQueue.size() > 0 ) {
			DropFrame(frame);
			continue;
		}

		ProcessFrame(frame);
	}

	TearDown();

}


void ProcessFrameTask::ProcessFrameMandatory(const Frame::Ptr & frame ) {

}

void ProcessFrameTask::ProcessFrame(const Frame::Ptr & frame) {

}

void ProcessFrameTask::DropFrame(const Frame::Ptr & frame) {

}

void ProcessFrameTask::TearDown() {

}

void ProcessFrameTask::QueueFrame( const Frame::Ptr & frame ) {
	d_frameQueue.push(frame);
}

void ProcessFrameTask::CloseFrameQueue() {
	d_frameQueue.push({});
}

} // namespace artemis
} // namespace fort
