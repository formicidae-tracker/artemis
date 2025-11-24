#pragma once

#include <gio/gio.h>
#include <glib.h>

#include <filesystem>

#include <readerwriterqueue.h>

#include <slog++/slog++.hpp>

namespace fort {
namespace artemis {
class MetadataFile {
public:
	MetadataFile(const std::filesystem::path &path);
	~MetadataFile();
	// disable copy and move
	MetadataFile(const MetadataFile &)            = delete;
	MetadataFile(MetadataFile &&)                 = delete;
	MetadataFile &operator=(const MetadataFile &) = delete;
	MetadataFile &operator=(MetadataFile &&)      = delete;

	void Write(uint64_t streamFrameID, uint64_t globalFrameID);

private:
	void scheduleDispatch();
	void dispatch();

	void incrementRef();
	void decrementRef();

	struct Association {
		uint64_t Stream, Global;
	};

	void writeAsync(const Association &a);

	slog::Logger<1>                            d_logger;
	moodycamel::ReaderWriterQueue<Association> d_queue;
	std::atomic<size_t>                        d_refCount{1};
	std::atomic<bool>  d_closing{false}, d_writing{false};
	GFile             *d_file{nullptr};
	GFileOutputStream *d_stream{nullptr};
	char               d_buffer[1024];
};

class MetadataHandler {
public:
#ifdef VIDEO_TEST_SMALL_BUFFER
	constexpr static size_t BUFFER_SIZE = 10;
	constexpr static size_t RB_SIZE     = 16;
#else
	constexpr static size_t BUFFER_SIZE = 30;
	constexpr static size_t RB_SIZE     = 64;
#endif
	constexpr static size_t RB_MASK = RB_SIZE - 1;

	static_assert(
	    RB_SIZE > 0 && (RB_SIZE & RB_MASK) == 0,
	    "RB_SIZE must be a power of two"
	);

	static_assert(
	    BUFFER_SIZE < (RB_SIZE - 1),
	    "RB_SIZE must be large enough to hold BUFFER_SIZE"
	);

	MetadataHandler();

	~MetadataHandler();

	MetadataHandler(const MetadataHandler &)            = delete;
	MetadataHandler(MetadataHandler &&)                 = delete;
	MetadataHandler &operator=(const MetadataHandler &) = delete;
	MetadataHandler &operator=(MetadataHandler &&)      = delete;

	void Register(uint64_t frameID, uint64_t PTS);
	void NewSegment(const std::filesystem::path &path, uint64_t startStreamPTS);

private:
	struct FrameAssociation {
		uint64_t FrameID, PTS;
	};

	void popUntilSizeIs(size_t count);

	inline size_t bufferSize() const;

	uint64_t peekPTS() const;

	uint64_t popFrameID();

	void enqueue(uint64_t frameID, uint64_t PTS);

	void popAndWrite();

	std::unique_ptr<MetadataFile> d_file;
	uint64_t                      d_framesWritten;
	std::optional<uint64_t>       d_registerPTSOffset, d_streamPTSOffset;
	std::vector<FrameAssociation> d_frames;
	size_t                        d_head{0}, d_tail{0};
	std::mutex                    d_mutex;
};

} // namespace artemis
} // namespace fort
