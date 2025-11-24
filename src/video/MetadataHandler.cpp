#include "MetadataHandler.hpp"

#include <cpptrace/cpptrace.hpp>

#include <fort/utils/Defer.hpp>

namespace fort {
namespace artemis {

MetadataFile::MetadataFile(const std::filesystem::path &path)
    : d_logger{slog::With(slog::String("metadata_file", path.string()))} {
	std::filesystem::create_directories(path.parent_path());

	// open the file for write at filepath,truncating any, and throw
	// cpptrace::runtime_error on issue.
	GError *error = nullptr;
	Defer {
		if (error != nullptr) {
			g_error_free(error);
		}
	};

	d_file = g_file_new_for_path(path.c_str());
	if (d_file == nullptr) {
		throw cpptrace::runtime_error(
		    "could not create GFile for '" + path.string() + "'"
		);
	}
	d_stream = g_file_replace(
	    d_file,
	    nullptr,
	    FALSE,
	    G_FILE_CREATE_NONE,
	    nullptr,
	    &error
	);
	if (d_stream == nullptr || error != nullptr) {
		throw cpptrace::runtime_error(
		    "could not open metadata file '" + path.string() + "': " +
		    std::string{error == nullptr ? "unknown error" : error->message}
		);
	}
	d_logger.Debug("opened");
}

MetadataFile::~MetadataFile() {
	d_logger.Debug("closing");
	d_closing.store(true);
	scheduleDispatch();
	decrementRef();
	size_t current;
	while ((current = d_refCount.load()) != 0) {
		d_refCount.wait(current);
	}
}

void MetadataFile::scheduleDispatch() {
	incrementRef();
	g_main_context_invoke(
	    g_main_context_default(),
	    [](gpointer userdata) {
		    auto self = reinterpret_cast<MetadataFile *>(userdata);
		    self->dispatch();
		    self->decrementRef();
		    return G_SOURCE_REMOVE;
	    },
	    this
	);
}

void MetadataFile::dispatch() {
	if (d_closing.load() == true) {
		if (d_stream == nullptr) {
			if (d_queue.size_approx() > 0) {
				d_logger.Error("loosing data as file is not opened on closing");
			}
			return;
		}
		if (d_writing.load() == true) {
			// write will re-dispatch
			return;
		}
		Association a;
		while (d_queue.try_dequeue(a)) {
			g_output_stream_printf(
			    G_OUTPUT_STREAM(d_stream),
			    nullptr,
			    nullptr,
			    nullptr,
			    "%lu %lu\n",
			    a.Stream,
			    a.Global
			);
		}

		g_output_stream_close(G_OUTPUT_STREAM(d_stream), nullptr, nullptr);
		g_object_unref(d_stream);
		g_object_unref(d_file);
		d_stream = nullptr;
		d_file   = nullptr;

		return;
	}

	if (d_writing.load() == true) {
		return;
	}
	Association a;
	if (d_queue.try_dequeue(a) == false) {
		return;
	}
	writeAsync(a);
}

void MetadataFile::writeAsync(const Association &a) {
	d_writing.store(true);
	auto size =
	    snprintf(d_buffer, sizeof(d_buffer), "%lu %lu\n", a.Stream, a.Global);
	incrementRef();
	g_output_stream_write_async(
	    G_OUTPUT_STREAM(d_stream),
	    d_buffer,
	    size,
	    G_PRIORITY_DEFAULT,
	    nullptr,
	    [](GObject *source_object, GAsyncResult *res, gpointer userdata) {
		    auto    self  = reinterpret_cast<MetadataFile *>(userdata);
		    GError *error = nullptr;
		    g_output_stream_write_finish(
		        G_OUTPUT_STREAM(source_object),
		        res,
		        &error
		    );
		    if (error != nullptr) {
			    self->d_logger.Error(
			        "could not write metadata: " + std::string{error->message}
			    );
			    g_error_free(error);
		    }
		    self->d_writing.store(false);
		    self->d_writing.notify_all();
		    self->dispatch();
		    self->decrementRef();
	    },
	    this
	);
}

void MetadataFile::incrementRef() {
	d_refCount.fetch_add(1);
}

void MetadataFile::decrementRef() {
	d_refCount.fetch_sub(1);
	d_refCount.notify_all();
}

void MetadataFile::Write(uint64_t streamFrameID, uint64_t globalFrameID) {
	if (d_closing.load() == true) {
		return;
	}
	d_queue.enqueue({.Stream = streamFrameID, .Global = globalFrameID});
	scheduleDispatch();
}

MetadataHandler::MetadataHandler() {
	d_frames.resize(RB_SIZE);
}

MetadataHandler::~MetadataHandler() {
	std::lock_guard<std::mutex> lock{d_mutex};

	if (d_file == nullptr) {
		return;
	}
	popUntilSizeIs(0);
}

void MetadataHandler::Register(uint64_t frameID, uint64_t PTS) {
	std::lock_guard<std::mutex> lock{d_mutex};

	if (d_registerPTSOffset.has_value() == false) {
		d_registerPTSOffset = frameID;
	}

	enqueue(frameID, PTS - d_registerPTSOffset.value());
	if (d_file == nullptr) {
		return;
	}
	popUntilSizeIs(BUFFER_SIZE);
}

void MetadataHandler::NewSegment(
    const std::filesystem::path &path, uint64_t startStreamPTS
) {
	std::lock_guard<std::mutex> lock{d_mutex};
	if (d_streamPTSOffset.has_value() == false) {
		d_streamPTSOffset = startStreamPTS;
	}
	startStreamPTS -= d_streamPTSOffset.value();

	if (d_file != nullptr) {
		while (bufferSize() > 0 && peekPTS() < startStreamPTS) {
			popAndWrite();
		}
	}
	d_file          = std::make_unique<MetadataFile>(path);
	d_framesWritten = 0;
	popUntilSizeIs(BUFFER_SIZE);
}

void MetadataHandler::popUntilSizeIs(size_t count) {
	while (bufferSize() > count) {
		popAndWrite();
	}
}

inline size_t MetadataHandler::bufferSize() const {
	return d_head - d_tail;
}

uint64_t MetadataHandler::peekPTS() const {
	return d_frames[d_tail & RB_MASK].PTS;
}

uint64_t MetadataHandler::popFrameID() {
	return d_frames[(d_tail++) & RB_MASK].FrameID;
}

void MetadataHandler::enqueue(uint64_t frameID, uint64_t PTS) {
	d_frames[(d_head++) & RB_MASK] = {.FrameID = frameID, .PTS = PTS};
}

void MetadataHandler::popAndWrite() {
	d_file->Write(d_framesWritten++, popFrameID());
}

} // namespace artemis
} // namespace fort
