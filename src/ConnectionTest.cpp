#include <gmock/gmock.h>

#include <gio/gio.h>
#include <glib.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <slog++/slog++.hpp>
#include <stdexcept>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/message.h>
#include <google/protobuf/util/delimited_message_util.h>

#include <fort/hermes/FrameReadout.pb.h>
#include <fort/hermes/Header.pb.h>

#include <fort/utils/Defer.hpp>

#include "Connection.hpp"

namespace fort {
namespace artemis {

class GZeroCopyInputStream
    : public ::google::protobuf::io::ZeroCopyInputStream {
public:
	GZeroCopyInputStream(GInputStream *stream, GCancellable *cancel)
	    : d_stream{stream}
	    , d_cancel{cancel} {
		if (d_stream == nullptr) {
			throw std::invalid_argument{"Cannot be nullstream"};
		}
		g_object_ref(d_stream);
	}

	~GZeroCopyInputStream() override {
		g_input_stream_close(d_stream, nullptr, nullptr);
		g_object_unref(&d_stream);
	}

	// forbids copy and move
	GZeroCopyInputStream(const GZeroCopyInputStream &other)            = delete;
	GZeroCopyInputStream(GZeroCopyInputStream &&other)                 = delete;
	GZeroCopyInputStream &operator=(const GZeroCopyInputStream &other) = delete;
	GZeroCopyInputStream &operator=(GZeroCopyInputStream &&other)      = delete;

	bool Next(const void **data, int *size) override {
		if (d_position == d_read) {
			try {
				readMore();
			} catch (const std::exception &e) {
				return false;
			}
		}
		if (d_position == d_read) {
			return false;
		}
		*data = d_buffer.data() + d_position;
		*size = d_position - d_read;
		return true;
	}

	void BackUp(int count) override {
		d_position -= std::min(size_t(count), d_position);
	}

	int64_t ByteCount() const override {
		return d_position;
	}

	bool Skip(int count) override {
		if (count < 0) {
			return false;
		}
		if (d_read > d_position) {
			auto skipable = std::min(size_t(count), d_read - d_position);
			count -= skipable;
			d_position += skipable;
		}

		if (count == 0) {
			return true;
		}
		return g_input_stream_skip(d_stream, count, nullptr, nullptr) > 0;
	}

private:
	void readMore() {
		constexpr gsize CHUNK_SIZE = 256;
		d_buffer.reserve(d_buffer.size() + CHUNK_SIZE);
		GError *error   = nullptr;
		gsize   newRead = g_input_stream_read(
            d_stream,
            d_buffer.data() + d_read,
            CHUNK_SIZE,
            d_cancel,
            &error
        );
		if (newRead < 0) {
			Defer {
				g_error_free(error);
			};
			throw std::runtime_error(
			    "read error: " + std::string(error->message)
			);
		}

		d_read += newRead;
	}

	GInputStream        *d_stream;
	GCancellable        *d_cancel;
	std::vector<uint8_t> d_buffer;
	size_t               d_read{0};
	size_t               d_position{0};
};

class LetoService {
public:
	constexpr static guint16 PORT = 12345;
	LetoService();
	~LetoService();

	LetoService(const LetoService &other)            = delete;
	LetoService(LetoService &&other)                 = delete;
	LetoService &operator=(const LetoService &other) = delete;
	LetoService &operator=(LetoService &&other)      = delete;

	MOCK_METHOD(size_t, OnConnection, (), ());

	MOCK_METHOD(void, OnHeader, (const fort::hermes::Header &header), ());
	MOCK_METHOD(
	    void, OnReadout, (const fort::hermes::FrameReadout &readout), ()
	);

	MOCK_METHOD(void, OnError, (), ());

	static int onConnection(
	    GSocketService    *service,
	    GSocketConnection *connection,
	    GObject           *source_object,
	    gpointer           userdata
	) {
		auto self       = reinterpret_cast<LetoService *>(userdata);
		auto toRead     = self->OnConnection();
		auto zeroStream = std::make_unique<GZeroCopyInputStream>(
		    g_io_stream_get_input_stream(G_IO_STREAM(connection)),
		    self->d_cancel
		);
		google::protobuf::io::CodedInputStream stream{zeroStream.get()};
		bool                                   cleanEOF;
		if (toRead > 0) {
			fort::hermes::Header header;
			bool good = google::protobuf::util::ParseDelimitedFromCodedStream(
			    &header,
			    &stream,
			    &cleanEOF
			);
			if (good) {
				self->OnHeader(header);
			}
		}
		for (size_t i = 0; i < toRead; ++i) {
			fort::hermes::FrameReadout readout;
			bool good = google::protobuf::util::ParseDelimitedFromCodedStream(
			    &readout,
			    &stream,
			    &cleanEOF
			);
			if (good) {
				self->OnReadout(readout);
			}
		}
		zeroStream.reset();
		return G_SOURCE_REMOVE;
	}

	GMainContext *Context() const {
		return d_context;
	}

protected:
	slog::Logger<1> d_logger;
	GMainContext   *d_context = nullptr;
	GMainLoop      *d_loop    = nullptr;
	GSocketService *d_service = nullptr;
	GCancellable   *d_cancel;

	std::thread d_mainLoopThread;
}; // namespace artemis

LetoService::LetoService()
    : d_logger{slog::With(slog::String("task", "letoService"))}
    , d_context{g_main_context_new()}
    , d_loop{g_main_loop_new(d_context, FALSE)}
    , d_service{} {

	g_main_context_push_thread_default(d_context);

	d_cancel  = g_cancellable_new();

	d_service = g_socket_service_new();

	d_logger.Info("service created");

	GError  *error;
	gboolean ok = g_socket_listener_add_inet_port(
	    G_SOCKET_LISTENER(d_service),
	    PORT,
	    nullptr,
	    &error
	);
	if (!ok) {
		g_main_context_pop_thread_default(d_context);

		g_object_unref(d_service);
		g_main_loop_unref(d_loop);
		g_main_context_unref(d_context);

		throw std::runtime_error(
		    "could not listen: " + std::string(error->message)
		);
	}
	d_logger.Info("listen added");

	g_signal_connect(d_service, "incoming", G_CALLBACK(onConnection), this);
	d_logger.Info("signal added");
	g_socket_service_start(d_service);
	d_logger.Info("service started");

	std::atomic<bool> ready{false};

	d_mainLoopThread = std::thread([this, &ready] {
		auto logger = slog::With(slog::String("task", "mainLoop"));
		g_main_context_invoke(
		    d_context,
		    [](gpointer userdata) -> gboolean {
			    auto ready = reinterpret_cast<std::atomic<bool> *>(userdata);
			    slog::Warn("signal ready");
			    ready->store(true);
			    ready->notify_all();
			    slog::Warn("signaled ready");
			    return G_SOURCE_REMOVE;
		    },
		    &ready
		);
		logger.Info("started");
		g_main_loop_run(d_loop);
		logger.Info("done");
	});

	ready.wait(false);
	d_logger.Info("ready");
}

LetoService::~LetoService() {
	g_cancellable_cancel(d_cancel);
	g_socket_service_stop(d_service);
	g_main_loop_quit(d_loop);
	d_mainLoopThread.join();
	g_object_unref(d_service);
	g_main_loop_unref(d_loop);
	g_main_context_unref(d_context);
}

class ConnectionTest : public testing::Test {
protected:
	void SetUp();
	void TearDown();

	std::unique_ptr<LetoService> d_service;
};

void ConnectionTest::SetUp() {
	d_service = std::make_unique<LetoService>();
}

void ConnectionTest::TearDown() {

	auto future =
	    std::async(std::launch::async, [this]() { d_service.reset(); });
	if (future.wait_for(std::chrono::milliseconds{200}) ==
	    std::future_status::timeout) {
		ADD_FAILURE() << "Timeouted on tear down";
	}
}

using ::testing::_;
using ::testing::Return;

TEST_F(ConnectionTest, Reconnect) {
	std::atomic<size_t> connections = 0;

	EXPECT_CALL(*d_service, OnConnection())
	    .Times(2)
	    .WillRepeatedly([&connections]() {
		    connections.fetch_add(1);
		    connections.notify_all();
		    return 0;
	    });
	auto connection = Connection(
	    d_service->Context(),
	    "localhost",
	    LetoService::PORT,
	    std::chrono::milliseconds{5}
	);
}

} // namespace artemis
} // namespace fort
