#include "gmock/gmock.h"
#include <atomic>
#include <glib-object.h>
#include <gmock/gmock.h>

#include <gio/gio.h>
#include <glib.h>

#include <chrono>
#include <cstdint>
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
#include <thread>

#include "Connection.hpp"

namespace fort {
namespace artemis {

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
		auto self = reinterpret_cast<LetoService *>(userdata);

		auto toRead = self->OnConnection();
		self->d_logger.Info("new connection", slog::Int("accepting", toRead));
		g_object_ref(connection);
		std::thread([self, toRead, connection]() {
			Defer {
				g_io_stream_close(
				    G_IO_STREAM(connection),
				    self->d_cancel,
				    nullptr
				);
				g_object_unref(connection);
				self->d_connectionClosed.fetch_add(1);
				self->d_connectionClosed.notify_all();
				self->d_logger.Info("closed");
			};
			if (toRead == 0) {
				self->d_logger.Info("reading none");
				return;
			}

			self->d_logger.DDebug("reading all");
			std::vector<uint8_t> buffer;
			constexpr size_t     BUFFER_SIZE = 1024;
			buffer.resize(BUFFER_SIZE);
			auto stream = g_io_stream_get_input_stream(G_IO_STREAM(connection));
			gsize read;
			auto  ok = g_input_stream_read_all(
                stream,
                buffer.data(),
                BUFFER_SIZE,
                &read,
                self->d_cancel,
                nullptr
            );

			self->d_logger.DDebug("reading all done", slog::Bool("ok", ok));
			if (!ok) {
				return;
			}
			buffer.resize(read);

			google::protobuf::io::CodedInputStream codedStream{
			    buffer.data(),
			    int(buffer.size())
			};

			bool cleanEOF;
			for (size_t i = 0; i < toRead; ++i) {
				fort::hermes::FrameReadout readout;
				bool                       good =
				    google::protobuf::util::ParseDelimitedFromCodedStream(
				        &readout,
				        &codedStream,
				        &cleanEOF
				    );
				if (good) {
					self->OnReadout(readout);
				}
			}
			self->d_logger.Info("Closing connection");
		}).detach();

		return G_SOURCE_REMOVE;
	}

	GMainContext *Context() const {
		return d_context;
	}

	const std::atomic<size_t> &ConnectionClosed() const {
		return d_connectionClosed;
	}

protected:
	slog::Logger<1> d_logger;
	GMainContext   *d_context = nullptr;
	GMainLoop      *d_loop    = nullptr;
	GSocketService *d_service = nullptr;
	GCancellable   *d_cancel;

	std::thread         d_mainLoopThread;
	std::atomic<bool>   d_ready{false};
	std::atomic<size_t> d_connectionClosed{0};
}; // namespace artemis

LetoService::LetoService()
    : d_logger{slog::With(slog::String("task", "letoService"))}
    , d_loop{nullptr}
    , d_service{nullptr} {

	d_mainLoopThread = std::thread([this] {
		d_context = g_main_context_new();
		d_loop    = g_main_loop_new(d_context, FALSE);
		g_main_context_push_thread_default(d_context);

		auto logger = slog::With(slog::String("task", "mainLoop"));
		logger.Info("started");
		auto source = g_idle_source_new();
		g_source_set_callback(
		    source,
		    [](gpointer userdata) {
			    auto self = reinterpret_cast<LetoService *>(userdata);
			    self->d_ready.store(true);
			    self->d_ready.notify_all();
			    return G_SOURCE_REMOVE;
		    },
		    this,
		    nullptr
		);
		g_source_attach(source, d_context);

		d_service = g_socket_service_new();

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
			g_main_context_unref(d_context);
			g_main_loop_unref(d_loop);
			d_service = nullptr;
			d_context = nullptr;
			d_loop    = nullptr;
			d_ready.store(true);
			d_ready.notify_all();
			return;
		}

		d_cancel = g_cancellable_new();
		g_signal_connect(d_service, "incoming", G_CALLBACK(onConnection), this);
		g_socket_service_start(d_service);
		d_logger.Info("service started");

		g_main_loop_run(d_loop);
		logger.Info("done");
	});

	d_ready.wait(false);
	if (d_service == nullptr) {
		d_mainLoopThread.join();
		throw std::runtime_error("could not initialize thread");
	}
	d_logger.Info("ready");
}

LetoService::~LetoService() {
	g_cancellable_cancel(d_cancel);
	g_object_unref(d_cancel);
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

	GMainLoop                   *d_loop;
	std::unique_ptr<LetoService> d_service;

	std::thread d_defaultMainLoop;
};

void ConnectionTest::SetUp() {
	d_loop = g_main_loop_new(nullptr, FALSE);
	std::atomic<bool> ready{false};
	g_timeout_add(
	    1,
	    [](gpointer udata) {
		    auto ready = reinterpret_cast<std::atomic<bool> *>(udata);
		    ready->store(true);
		    ready->notify_all();
		    return G_SOURCE_REMOVE;
	    },
	    &ready
	);
	d_defaultMainLoop = std::thread([this]() {
		g_main_loop_run(d_loop);
		g_main_loop_unref(d_loop);
	});
	ready.wait(false);
	d_service = std::make_unique<LetoService>();
}

void ConnectionTest::TearDown() {
	g_main_loop_quit(d_loop);
	d_defaultMainLoop.join();
	auto future =
	    std::async(std::launch::async, [this]() { d_service.reset(); });
	if (future.wait_for(std::chrono::milliseconds{200}) ==
	    std::future_status::timeout) {
		ADD_FAILURE() << "Timeouted on tear down";
		new std::future<void>(std::move(future)); // intentional leaking.
	}
}

using namespace std::chrono_literals;

TEST_F(ConnectionTest, Reconnect) {
	std::atomic<size_t> connections = 0;

	EXPECT_CALL(*d_service, OnConnection())
	    .Times(::testing::AnyNumber())
	    .WillRepeatedly([&connections]() {
		    connections.fetch_add(1);
		    connections.notify_all();
		    return 0;
	    });
	auto connection = Connection(
	    nullptr,
	    "localhost",
	    LetoService::PORT,
	    std::chrono::milliseconds{5}
	);

	auto future =
	    std::async(std::launch::async, [&connections, &connection, this]() {
		    for (size_t i = 0; i < 5; ++i) {
			    slog::Info("waiting connection", slog::Int("iter", i));
			    connections.wait(i);
			    d_service->ConnectionClosed().wait(i);
			    fort::hermes::FrameReadout ro;
			    ro.set_frameid(2 * i);
			    connection.PostMessage(ro);
			    ro.set_frameid(2 * i + 1);
			    connection.PostMessage(ro);
		    };

		    ASSERT_EQ(connections.load(), 5);
	    });

	if (future.wait_for(1000ms) == std::future_status::timeout) {
		ADD_FAILURE() << "Reconnection timeouted";
		new std::future<void>(std::move(future)); // intentionally leaking.
		return;
	}
}

using ::testing::Property;

TEST_F(ConnectionTest, DoesNotMangle) {
	constexpr size_t    SEQUENCE_SIZE = 100;
	std::atomic<size_t> connections{0};
	std::atomic<size_t> reads{0};
	EXPECT_CALL(*d_service, OnConnection())
	    .Times(::testing::AnyNumber())
	    .WillRepeatedly([&connections]() -> int {
		    connections.fetch_add(1);
		    connections.notify_all();
		    return SEQUENCE_SIZE;
	    });

	{
		::testing::InSequence seq;
		for (size_t i = 0; i < SEQUENCE_SIZE; ++i) {
			EXPECT_CALL(
			    *d_service,
			    OnReadout(Property(&hermes::FrameReadout::frameid, i + 1))
			)
			    .WillOnce([&reads]() {
				    reads.fetch_add(1);
				    reads.notify_all();
			    });
		}
	}
	auto connection = Connection(
	    nullptr,
	    "localhost",
	    LetoService::PORT,
	    std::chrono::milliseconds{5}
	);

	connections.wait(0);
	fort::hermes::FrameReadout ro;
	for (size_t i = 0; i < SEQUENCE_SIZE; ++i) {
		ro.set_frameid(i + 1);
		ASSERT_TRUE(connection.PostMessage(ro));
		std::this_thread::sleep_for(std::chrono::milliseconds{1});
	}
	// needed as our implementation of LetoService reads the full stream of
	// data.
	connection.Close();

	auto future = std::async(std::launch::async, [&reads]() {
		auto current = reads.load();
		while (current != SEQUENCE_SIZE) {
			reads.wait(current);
			current = reads.load();
		}
	});
	if (future.wait_for(1000ms) == std::future_status::timeout) {
		ADD_FAILURE() << "Write timeouted";
		new std::future<void>(std::move(future)); // intentional leak.
	}
}

} // namespace artemis
} // namespace fort
