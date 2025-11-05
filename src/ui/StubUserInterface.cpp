#include "StubUserInterface.hpp"
#include <slog++/slog++.hpp>

namespace fort {
namespace artemis {

StubUserInterface::StubUserInterface(
    const Size          &workingResolution,
    const Options       &options,
    const ROIChannelPtr &roiChannel
)
    : UserInterface(workingResolution, options, roiChannel) {}

void StubUserInterface::Task() {}

void StubUserInterface::
    UpdateFrame(const FrameToDisplay &frame, const DataToDisplay &) {
	slog::Info(
	    "Displaying a new frame",
	    slog::Int("processed", frame.FrameProcessed),
	    slog::Int("dropped", frame.FrameDropped),
	    slog::Float("FPS", frame.FPS),
	    slog::Int("quads", frame.Message->quads())
	);
}

} // namespace artemis
} // namespace fort
