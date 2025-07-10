#pragma once

#include "Task.hpp"

#include <readerwriterqueue.h>

#include "FrameGrabber.hpp"

namespace fort {
namespace artemis {

class FullFrameExportTask : public Task {
public:
	FullFrameExportTask(const std::string &dir);
	virtual ~FullFrameExportTask();

	void Run();

	void CloseQueue();

	bool QueueExport(const Frame::Ptr &frame);

	bool IsFree() const;

private:
	static void ExportFrame(const std::string &dir, const Frame::Ptr &frame);

	moodycamel::BlockingReaderWriterQueue<Frame::Ptr> d_queue;
	std::string                                       d_dir;
};

} // namespace artemis
} // namespace fort
