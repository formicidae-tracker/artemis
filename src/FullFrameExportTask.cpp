#include "FullFrameExportTask.hpp"

#include <opencv2/imgcodecs.hpp>

#include <glog/logging.h>

namespace fort {
namespace artemis {

FullFrameExportTask::FullFrameExportTask(const std::string & dir)
	: d_dir(dir) {
	if ( dir.empty() ) {
		throw std::invalid_argument("No directory for output");
	}
}

FullFrameExportTask::~FullFrameExportTask() {}

void FullFrameExportTask::Run() {
	LOG(INFO) << "[FullFrameExportTask]: started";
	for(;;) {
		Frame::Ptr f;
		d_queue.pop(f);
		if (!f) {
			break;
		}
		ExportFrame(d_dir,f);
	}
	LOG(INFO) << "[FullFrameExportTask]: ended";
}

void FullFrameExportTask::CloseQueue() {
	LOG(INFO) << "Queue size " << d_queue.size();
	d_queue.push(nullptr);
}

bool FullFrameExportTask::QueueExport(const Frame::Ptr & frame) {
	if ( d_queue.size() >= 0 ) {
		return false;
	}
	return d_queue.try_push(frame);
}

bool FullFrameExportTask::IsFree() const {
	return d_queue.size() < 0;
}

void FullFrameExportTask::ExportFrame(const std::string & dir,
                                      const Frame::Ptr & frame) {
	std::ostringstream oss;
	oss << dir << "/frame_" << frame->ID() << ".png";
	LOG(INFO) << "Exporting to "  << oss.str();
	cv::imwrite(oss.str(),frame->ToCV());
	LOG(INFO) << "Done";
}


} // namespace artemis
} // namespace fort
