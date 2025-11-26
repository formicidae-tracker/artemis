#pragma once

#include "BusManagedPipeline.hpp"
#include "Options.hpp"
#include "VideoOutput.hpp"
#include "video/MetadataHandler.hpp"
#include "video/gstreamer.hpp"
#include <cstdint>

namespace fort {
namespace artemis {

class FilePipeline : public BusManagedPipeline {
public:
	FilePipeline(
	    const VideoOutputOptions &options, const VideoOutput::Config &config
	);

	virtual ~FilePipeline();

	FilePipeline(const FilePipeline &)            = delete;
	FilePipeline(FilePipeline &&)                 = delete;
	FilePipeline &operator=(const FilePipeline &) = delete;
	FilePipeline &operator=(FilePipeline &&)      = delete;

	bool PushFrame(
	    const Frame::Ptr &frame, const std::shared_ptr<FilePipeline> &self
	);

private:
	friend class VideoOutputImpl;

	static gchararray onLocationFull(
	    GstElement *filesink,
	    guint       fragment_id,
	    GstSample  *sample,
	    gpointer    userdata
	);

	static std::string buildPipelineDescription(
	    const VideoOutputOptions &options, const VideoOutput::Config &config
	);

	void onFramePass(uint64_t frameID, uint64_t PTS);
	void onFrameDone(uint64_t frameID);
	void notifyDrop(uint64_t frameID);

	void configureTimestampOverlay();
	void beforeFrameTimestamp(uint64_t PTS);

	std::filesystem::path d_outputFileTemplate{};
	MetadataHandler       d_metadata;

	GstElementPtr d_inputSrc, d_splitMuxSink, d_fileConvert, d_fileTextoverlay;
	GstPadPtr     d_inputSrc_src, d_fileConvert_src;

	std::atomic<bool> d_closing{false};

	std::atomic<uint64_t>   d_lastFramePassed{0}, d_processed{0}, d_dropped{0};
	std::optional<Time>     d_streamStart;
};
} // namespace artemis
} // namespace fort
