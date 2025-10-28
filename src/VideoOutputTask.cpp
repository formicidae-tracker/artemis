#include "VideoOutputTask.hpp"

#include "utils/PosixCall.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/write.hpp>

#include "ImageTextRenderer.hpp"

#include <iomanip>
#include <slog++/slog++.hpp>

namespace fort {
namespace artemis {

VideoOutputTask::VideoOutputTask(
    const VideoOutputOptions &options,
    boost::asio::io_context  &context,
    bool                      legacyMode
)
    : d_stream(context, STDOUT_FILENO)
    , d_done(true)
    , d_addHeader(options.AddHeader)
    , d_legacyMode(legacyMode)
    , d_logger{slog::With(slog::String("task", "VideoOutput"))} {

	int flags = fcntl(STDOUT_FILENO, F_GETFL);
	if (flags == -1) {
		throw ARTEMIS_SYSTEM_ERROR(fcntl - get, errno);
	}
	p_call(fcntl, STDOUT_FILENO, F_SETFL, flags | O_NONBLOCK);
}

VideoOutputTask::~VideoOutputTask() {}

void VideoOutputTask::QueueFrame(
    const std::shared_ptr<cv::Mat> &image,
    const Time                     &frameTime,
    const uint64_t                  frameID
) {
	d_queue.enqueue({image, frameTime, frameID});
}

void VideoOutputTask::CloseQueue() {
	d_queue.enqueue({nullptr, Time(), 0});
}

void VideoOutputTask::Run() {
	d_logger.Info("started");

	d_frameDropped.store(0);
	d_frameProcessed.store(0);

	FrameData data;
	size_t    frameDropped(0), frameProcessed(0);
	for (;;) {
		d_queue.wait_dequeue(data);
		const auto &[imagePtr, time, frameID] = data;
		if (!imagePtr) {
			break;
		}

		{
			std::lock_guard<std::mutex> lock(d_mutex);
			if (d_queue.peek() != nullptr || d_done == false) {
				frameDropped = d_frameDropped.fetch_add(1) + 1;
				d_logger.Error(
				    "dropping frame",
				    slog::Int("dropped_count", frameDropped),
				    slog::Float(
				        "dropped_percent",
				        100.0 * float(frameDropped) /
				            float(frameDropped + frameProcessed)
				    )
				);
				continue;
			}
			d_done = false;
		}

		OverlayData(*imagePtr, time, frameID);

		OutputData(imagePtr, frameID);
		frameProcessed = d_frameProcessed.fetch_add(1) + 1;
	}
	d_logger.Info("ended");
}

void VideoOutputTask::OverlayData(
    cv::Mat &frame, const Time &time, uint64_t frameID
) {
	if (d_legacyMode == true) {
		OverlayFrameNumber(frame, frameID);
		OverlayLegacyTime(frame, time);
		return;
	}
	OverlayTime(frame, time);
}

void VideoOutputTask::OverlayFrameNumber(cv::Mat &frame, uint64_t frameID) {
	std::ostringstream os;
	os << "Frame " << std::setw(8) << std::setfill('0') << frameID;
	ImageTextRenderer::RenderText(frame, os.str(), {0, 0});
}

void VideoOutputTask::OverlayLegacyTime(cv::Mat &frame, const Time &time) {
	std::ostringstream oss;
	auto               fTime = time.ToTimeT();
	oss << std::put_time(std::localtime(&fTime), "%c %Z");
	ImageTextRenderer::RenderText(
	    frame,
	    oss.str(),
	    {frame.cols, 0},
	    ImageTextRenderer::RIGHT_ALIGNED
	);
}

void VideoOutputTask::OverlayTime(cv::Mat &frame, const Time &time) {
	auto               rounded = time.Round(Duration::Millisecond);
	std::ostringstream oss;
	oss << rounded;
	ImageTextRenderer::RenderText(
	    frame,
	    oss.str(),
	    {frame.cols, 0},
	    ImageTextRenderer::RIGHT_ALIGNED
	);
}

void VideoOutputTask::OutputData(
    const std::shared_ptr<cv::Mat> &framePtr, uint64_t frameID
) {

	if (d_addHeader) {
		d_headerData.resize(3);

		d_headerData[0] = frameID;
		d_headerData[1] = framePtr->cols;
		d_headerData[2] = framePtr->rows;

		boost::asio::async_write(
		    d_stream,
		    boost::asio::const_buffer(&(d_headerData[0]), sizeof(uint64_t) * 3),
		    [this, framePtr](const boost::system::error_code &ec, std::size_t) {
			    if (ec) {
				    d_logger.Error(
				        "could not write header data to STDOUT",
				        slog::Int("error_code", ec.value())
				    );
				    MarkDone();
			    } else {
				    OutputDataWithoutHeader(framePtr);
			    }
		    }
		);

	} else {
		OutputDataWithoutHeader(framePtr);
	}
}

void VideoOutputTask::MarkDone() {
	std::lock_guard<std::mutex> lock(d_mutex);
	d_done = true;
}

void VideoOutputTask::OutputDataWithoutHeader(
    const std::shared_ptr<cv::Mat> &framePtr
) {
	boost::asio::async_write(
	    d_stream,
	    boost::asio::const_buffer(
	        framePtr->datastart,
	        framePtr->dataend - framePtr->datastart
	    ),
	    [this, framePtr](const boost::system::error_code &ec, size_t) {
		    if (ec) {
			    d_logger.Error(
			        "could not write data to STDOUT",
			        slog::Int("error_code", ec.value())
			    );
		    }
		    MarkDone();
	    }
	);
}

size_t VideoOutputTask::FrameProcessed() const {
	return d_frameProcessed.load();
}

size_t VideoOutputTask::FrameDropped() const {
	return d_frameDropped.load();
}

} // namespace artemis
} // namespace fort
