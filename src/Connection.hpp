#pragma once

#include <atomic>
#include <concurrentqueue.h>

#include <google/protobuf/message.h>

#include <fort/time/Time.hpp>

#include <slog++/Logger.hpp>

#include <gio/gio.h>
#include <glib.h>

namespace fort {
namespace artemis {

class Connection {
public:
	~Connection();

	Connection(
	    GMainContext      *context,
	    const std::string &host,
	    uint16_t           port,
	    Duration           reconnectPeriod
	);

	Connection(const Connection &other)            = delete;
	Connection(Connection &&other)                 = delete;
	Connection &operator=(const Connection &other) = delete;
	Connection &operator=(Connection &&other)      = delete;

	void Close();

	// thread-safe function
	bool PostMessage(const google::protobuf::MessageLite &m, uint64_t frameID);

private:
	using Queue = moodycamel::ConcurrentQueue<std::string>;

	static gboolean mainLoopDispatchCb(gpointer userdata);
	void            mainLoopDispatch();
	void            scheduleDispatch();

	void connectAsync();
	void scheduleReconnect();
	void closeConnection();

	void sendNextBuffer();

	void startClosing();
	void waitClosed();

	void incrementRefcount();
	void decrementRefcount();

	const std::string d_host;
	const uint16_t    d_port;
	const Duration    d_reconnectPeriod;
	slog::Logger<1>   d_logger;

	GMainContext      *d_context;
	GSocketClient     *d_client;
	GSocketConnection *d_connection;
	GOutputStream     *d_stream;

	guint d_reconnectionSource{0};

	Queue d_queue;

	enum class State {
		INITIAL,
		CONNECTING,
		CONNECTED,
		WRITING,
		CLOSED,
	};
	std::atomic<bool>   d_closing{false};
	std::atomic<State>  d_state{State::INITIAL};
	std::atomic<size_t> d_txID{0};
	std::atomic<size_t> d_refCount{1};
};

} // namespace artemis
} // namespace fort
