#pragma once

#include "Task.hpp"

#include "FrameGrabber.hpp"

#include <tbb/concurrent_queue.h>


namespace fort {
namespace artemis {

class FullFrameExportTask : public Task {
public:
	FullFrameExportTask(const std::string & dir);
	virtual ~FullFrameExportTask();

	void Run();

	void CloseQueue();

	bool QueueExport(const Frame::Ptr & frame);

	bool IsFree() const;

private:
	static void ExportFrame(const std::string & dir,
	                        const Frame::Ptr & frame);

	tbb::concurrent_bounded_queue<Frame::Ptr> d_queue;
	std::string d_dir;
};

} // namespace artemis
} // namespace fort
