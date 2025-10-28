#pragma once

#include "Task.hpp"

#include <filesystem>
#include <readerwriterqueue.h>
#include <slog++/Logger.hpp>

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
	void ExportFrame(const std::filesystem::path &dir, const Frame::Ptr &frame);

	moodycamel::BlockingReaderWriterQueue<Frame::Ptr> d_queue;
	std::filesystem::path                             d_dir;
	slog::Logger<1>                                   d_logger;
};

} // namespace artemis
} // namespace fort
