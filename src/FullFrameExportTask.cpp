#include "FullFrameExportTask.hpp"

#include <filesystem>
#include <opencv2/imgcodecs.hpp>
#include <slog++/slog++.hpp>

namespace fort {
namespace artemis {

FullFrameExportTask::FullFrameExportTask(const std::string &dir)
    : d_dir(dir)
    , d_logger{slog::With(slog::String("task", "FullFrameExport"))} {
	if (dir.empty()) {
		throw std::invalid_argument("No directory for output");
	}
	std::filesystem::create_directories(d_dir);
}

FullFrameExportTask::~FullFrameExportTask() {}

void FullFrameExportTask::Run() {
	d_logger.Info("started");

	for (;;) {
		Frame::Ptr f;
		d_queue.wait_dequeue(f);
		if (!f) {
			break;
		}
		ExportFrame(d_dir, f);
	}
	d_logger.Info("ended");
}

void FullFrameExportTask::CloseQueue() {
	d_logger.Info("on close", slog::Int("size", d_queue.size_approx()));
	d_queue.enqueue(nullptr);
}

bool FullFrameExportTask::QueueExport(const Frame::Ptr &frame) {
	if (d_queue.peek() != nullptr) {
		return false;
	}
	return d_queue.enqueue(frame);
}

bool FullFrameExportTask::IsFree() const {
	return d_queue.peek() == nullptr;
}

void FullFrameExportTask::ExportFrame(
    const std::filesystem::path &dir, const Frame::Ptr &frame
) {

	auto dest = dir / ("frame_" + std::to_string(frame->ID()) + ".png");

	auto logger = d_logger.With(slog::String("dest", dest.string()));
	logger.Info("exporting");
	cv::imwrite(dest.string(), frame->ToCV());
	logger.Info("exporting done");
}

} // namespace artemis
} // namespace fort
