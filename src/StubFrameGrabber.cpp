#include "StubFrameGrabber.hpp"
#include "ImageU8.hpp"

#include <taskflow/algorithm/for_each.hpp>
#include <taskflow/taskflow.hpp>

#include <unistd.h>

namespace fort {
namespace artemis {

StubFrame::StubFrame(const ImageU8 &image, uint64_t ID)
    : d_ID(ID)
    , d_image{image} {}

StubFrame::~StubFrame() {}

void *StubFrame::Data() {
	return d_image.buffer;
}

size_t StubFrame::Width() const {
	return d_image.width;
}

size_t StubFrame::Height() const {
	return d_image.height;
}

uint64_t StubFrame::Timestamp() const {
	return Time().MonotonicValue() / 1000;
}

uint64_t StubFrame::ID() const {
	return d_ID;
}

ImageU8 StubFrame::ToImageU8() {
	return ImageU8{d_image};
}

StubFrameGrabber::StubFrameGrabber(
    const std::vector<std::string> &paths, double FPS
)
    : d_ID(0)
    , d_timestamp(0)
    , d_period(1.0e9 / FPS) {
	if (paths.empty() == true) {
		throw std::invalid_argument("No paths given to StubFrameGrabber");
	}
	tf::Executor exec;
	tf::Taskflow taskflow;

	d_images.resize(paths.size());
	taskflow.for_each_index(0U, paths.size(), 1U, [this, &paths](size_t i) {
		d_images[i] = ImageU8::ReadPNG(paths[i]);
		if (d_images[i]->buffer == NULL) {
			throw std::runtime_error("Could not load '" + paths[i] + "'");
		}
	});

	auto fut = exec.run(taskflow);
	// needed for exception
	fut.get();

	size_t i = -1;
	for (const auto &img : d_images) {
		++i;
		if (img->Size() != d_images[0]->Size()) {
			throw std::runtime_error(
			    "'" + paths[0] + "' and '" + paths[i] + "' have different sizes"
			);
		}
	}
}

StubFrameGrabber::~StubFrameGrabber() {}

void StubFrameGrabber::Start() {
	d_last = Time::Now().Add(-d_period);
}

void StubFrameGrabber::Stop() {}

Size StubFrameGrabber::Resolution() const {
	return {d_images.front()->width, d_images.front()->height};
}

Frame::Ptr StubFrameGrabber::NextFrame() {
	auto toWait = d_last.Add(d_period).Sub(Time::Now());
	if (toWait > 0) {
		usleep(toWait.Microseconds());
	}

	Frame::Ptr res =
	    std::make_shared<StubFrame>(*d_images[d_ID % d_images.size()], d_ID);
	d_ID += 1;
	d_last = res->Time();
	return res;
}

} // namespace artemis
} // namespace fort
