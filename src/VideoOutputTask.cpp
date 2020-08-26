#include "VideoOutputTask.hpp"

#include "utils/PosixCall.hpp"

#include <glog/logging.h>

#include <boost/asio/write.hpp>
#include <boost/asio/buffer.hpp>

#include "ImageTextRenderer.hpp"

namespace fort {
namespace artemis {

VideoOutputTask::VideoOutputTask(const VideoOutputOptions & options,
                                 boost::asio::io_context & context,
                                 bool legacyMode)
	: d_stream(context,STDOUT_FILENO)
	, d_done(true)
	, d_addHeader(options.AddHeader)
	, d_legacyMode(legacyMode) {

	int flags = fcntl(STDOUT_FILENO, F_GETFL);
	if ( flags == -1 ) {
		throw ARTEMIS_SYSTEM_ERROR(fcntl-get,errno);
	}
	p_call(fcntl,STDOUT_FILENO,F_SETFL,flags | O_NONBLOCK);

}


VideoOutputTask::~VideoOutputTask() {
}


void VideoOutputTask::QueueFrame(const std::shared_ptr<cv::Mat> & image,
                                 const Time & frameTime,
                                 const uint64_t frameID) {
	d_queue.push({image,frameTime,frameID});
}

void VideoOutputTask::CloseQueue() {
	d_queue.push({nullptr,Time(),0});
}


void VideoOutputTask::Run() {
	LOG(INFO) << "[VideoOutputTask]: Started";

	FrameData data;

	for (;;) {
		d_queue.pop(data);
		const auto & [imagePtr,time,frameID] = data;
		if ( !imagePtr ) {
			break;
		}

		{
			std::lock_guard<std::mutex> lock(d_mutex);
			if ( d_queue.size() > 1
			     || d_done == false ) {
				LOG(ERROR) << "[VideoOutput]: dropping frame " << frameID;
				continue;
			}
			d_done = false;
		}

		OverlayData(*imagePtr,time,frameID);

		OutputData(imagePtr,frameID);
	}
	LOG(INFO) << "[VideoOutputTask]: Ended";
}

void VideoOutputTask::OverlayData(cv::Mat & frame,
                                  const Time & time,
                                  uint64_t frameID) {
	if ( d_legacyMode == true ) {
		OverlayFrameNumber(frame,frameID);
		OverlayLegacyTime(frame,time);
	}
	OverlayTime(frame,time);
}

void VideoOutputTask::OverlayFrameNumber(cv::Mat & frame,
                                         uint64_t frameID) {
	std::ostringstream os;
	os << "Frame "  << std::setw(8) << std::setfill('0') << frameID;
	ImageTextRenderer::RenderText(frame,os.str(),{0,0});
}


void VideoOutputTask::OverlayLegacyTime(cv::Mat & frame,
                                        const Time & time) {
	std::ostringstream oss;
	auto fTime = time.ToTimeT();
	oss << std::put_time(std::localtime(&fTime),"%c %Z");
	ImageTextRenderer::RenderText(frame,oss.str(),{frame.cols,0},ImageTextRenderer::RIGHT_ALIGNED);
}


void VideoOutputTask::OverlayTime(cv::Mat & frame,
                                  const Time & time) {
	auto rounded = time.Round(Duration::Millisecond);
	std::ostringstream oss;
	oss << rounded;
	ImageTextRenderer::RenderText(frame,oss.str(),{frame.cols,0},ImageTextRenderer::RIGHT_ALIGNED);
}



void VideoOutputTask::OutputData(const std::shared_ptr<cv::Mat> & framePtr,
                                 uint64_t frameID) {

	if ( d_addHeader ) {
		d_headerData.resize(3);

		d_headerData[0] = frameID;
		d_headerData[1] = framePtr->cols;
		d_headerData[2] = framePtr->rows;

		boost::asio::async_write(d_stream,
		                         boost::asio::const_buffer(&(d_headerData[0]),sizeof(uint64_t)*3),
		                         [this,framePtr] (const boost::system::error_code & ec,
		                                          std::size_t ) {
			                         if ( ec ) {
				                         LOG(ERROR) << "[VideoOutput]: could not write header data to STDOUT: " << ec;
				                         MarkDone();
			                         } else {
				                         OutputDataWithoutHeader(framePtr);
			                         }
		                         });

	} else {
		OutputDataWithoutHeader(framePtr);
	}

}

void VideoOutputTask::MarkDone() {
	std::lock_guard<std::mutex> lock(d_mutex);
	d_done = true;
}


void VideoOutputTask::OutputDataWithoutHeader(const std::shared_ptr<cv::Mat> & framePtr) {
	boost::asio::async_write(d_stream,
	                         boost::asio::const_buffer(framePtr->datastart,framePtr->dataend-framePtr->datastart),
	                         [this,framePtr] (const boost::system::error_code & ec,size_t) {
		                         if ( ec ) {
			                         LOG(ERROR) << "[VideoOutput]: could not write data to STDOUT: " << ec;
		                         }
		                         MarkDone();
	                         });
}



} // namespace artemis
} // namespace fort
