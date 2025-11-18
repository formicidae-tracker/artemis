#include "Connection.hpp"

#include <cpptrace/basic.hpp>
#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>

#include <sstream>

#include <google/protobuf/message_lite.h>
#include <google/protobuf/util/delimited_message_util.h>

#include <magic_enum/magic_enum.hpp>

#include <slog++/slog++.hpp>

#include <cpptrace/cpptrace.hpp>

#include <fort/utils/Defer.hpp>

namespace fort {
namespace artemis {
using namespace std::chrono_literals;

template <typename T>
void atomic_wait_for_value(std::atomic<T> &a, T newValue) {
	T current = a.load();
	while (current != newValue) {
		a.wait(current);
		// wait for a change.
		current = a.load();
	}
}

Connection::~Connection() {
	Close();
	// The refcount is needed as scheduled dispatch may still need this object
	// to be alive.
	decrementRefcount();
	atomic_wait_for_value<size_t>(d_refCount, 0);
}

Connection::Connection(
    GMainContext      *context,
    const std::string &host,
    uint16_t           port,
    Duration           reconnectPeriod
)
    : d_host{host}
    , d_port{port}
    , d_reconnectPeriod{reconnectPeriod}
    , d_logger{slog::With(slog::Group(
          "connection", slog::String("host", host), slog::Int("port", port)
      ))}
    , d_context{context == nullptr ? g_main_context_default() : context}
    , d_client{nullptr}
    , d_connection{nullptr}
    , d_stream{nullptr}
    , d_queue{64} {
	scheduleDispatch();
}

void Connection::Close() {
	incrementRefcount();
	g_main_context_invoke(
	    d_context,
	    [](gpointer userdata) {
		    auto self = reinterpret_cast<Connection *>(userdata);
		    if (self->d_reconnectionSource == 0) {
			    self->decrementRefcount(); // for myself.
			    return G_SOURCE_REMOVE;
		    }
		    g_source_remove(self->d_reconnectionSource);
		    self->decrementRefcount(); // for reconnection callback
		    self->decrementRefcount(); // for myself.
		    return G_SOURCE_REMOVE;
	    },
	    this
	);

	startClosing();
	waitClosed();
}

gboolean Connection::mainLoopDispatchCb(gpointer userdata) {
	auto self = reinterpret_cast<Connection *>(userdata);
	self->mainLoopDispatch();
	return G_SOURCE_REMOVE;
}

void Connection::closeConnection() {
	d_stream = nullptr;
	if (d_connection != nullptr) {
		g_io_stream_close(G_IO_STREAM(d_connection), nullptr, nullptr);
		g_clear_object(&d_connection);
	}
	if (d_client != nullptr) {
		g_clear_object(&d_client);
	}
}

void Connection::mainLoopDispatch() {
	d_logger.DDebug(
	    "dispatch",
	    slog::String(
	        "state",
	        std::string(magic_enum::enum_name(d_state.load()))
	    ),
	    slog::Int("refCount", d_refCount.load())
	);
	Defer {
		d_logger.DDebug(
		    "dispatch done",
		    slog::String(
		        "state",
		        std::string(magic_enum::enum_name(d_state.load()))
		    )
		);
		decrementRefcount();
	};

	if (d_closing.load()) {
		if (d_stream == nullptr && d_queue.size_approx() > 0) {
			d_logger.Warn(
			    "dropping message queue as not connected on close",
			    slog::Int("size", d_queue.size_approx())
			);

			std::string buf;
			while (d_queue.try_dequeue(buf)) {
			}
		}

		if (d_queue.size_approx() == 0) {
			closeConnection();
			d_state.store(State::CLOSED);
			d_state.notify_all();
			return;
		}
	}

	switch (d_state.load()) {
	case State::INITIAL:
		connectAsync();
		break;
	case State::CONNECTING:
	case State::WRITING:
	case State::CLOSED:
		break;
	case State::CONNECTED:
		sendNextBuffer();
		break;
	}
}

void Connection::connectAsync() {
	if (d_state.load() != State::INITIAL || d_closing.load() == true) {
		return;
	}

	if (d_client == nullptr) {
		d_client = g_socket_client_new();
	}
	d_state.store(State::CONNECTING);
	d_logger.Info("connecting");
	incrementRefcount();
	g_socket_client_connect_to_host_async(
	    d_client,
	    d_host.c_str(),
	    d_port,
	    nullptr,
	    [](GObject *source, GAsyncResult *res, gpointer data) {
		    auto self = reinterpret_cast<Connection *>(data);
		    Defer {
			    self->decrementRefcount();
		    };
		    GError            *error = nullptr;
		    GSocketConnection *connection =
		        G_SOCKET_CONNECTION(g_socket_client_connect_finish(
		            G_SOCKET_CLIENT(source),
		            res,
		            &error
		        ));

		    if (connection == nullptr) {
			    self->d_logger.Error(
			        "connection failed",
			        slog::String(
			            "error",
			            error == nullptr ? "unknown" : error->message
			        )
			    );
			    if (error != nullptr) {
				    g_error_free(error);
			    }
			    self->scheduleReconnect();
			    return;
		    }

		    if (self->d_connection != nullptr) {
			    g_io_stream_close(
			        G_IO_STREAM(self->d_connection),
			        nullptr,
			        nullptr
			    );
			    g_clear_object(&self->d_connection);
		    }
		    self->d_connection = connection;
		    self->d_stream =
		        g_io_stream_get_output_stream(G_IO_STREAM(connection));
		    self->d_state.store(State::CONNECTED);
		    self->d_logger.Info("connected");
		    self->sendNextBuffer();
	    },
	    this
	);
}

void Connection::scheduleReconnect() {
	if (d_reconnectionSource != 0) {
		d_logger.Warn("double reconnection attempt");
		return;
	}

	d_state.store(State::CONNECTING);
	incrementRefcount();
	auto source = g_timeout_source_new(guint(d_reconnectPeriod.Milliseconds()));

	g_source_set_callback(
	    source,
	    [](gpointer userData) -> gboolean {
		    auto self = reinterpret_cast<Connection *>(userData);
		    self->d_state.store(State::INITIAL);
		    self->connectAsync();
		    self->decrementRefcount();
		    self->d_reconnectionSource = 0;
		    return G_SOURCE_REMOVE;
	    },
	    this,
	    nullptr
	);
	g_source_attach(source, d_context);
	d_reconnectionSource = g_source_get_id(source);

	g_source_unref(source);
}

void Connection::sendNextBuffer() {

	if (d_state.load() != State::CONNECTED) {

		return;
	}

	std::string next;
	if (d_queue.try_dequeue(next) == false) {
		return;
	}

	d_state.store(State::WRITING);
	d_logger.DDebug(
	    "writing",
	    slog::Int("total", next.size()),
	    slog::Int("txID", d_txID.fetch_add(1) + 1)
	);
	auto cancel = g_cancellable_new();
	g_timeout_add(
	    2000,
	    [](gpointer userdata) {
		    auto cancel = reinterpret_cast<GCancellable *>(userdata);
		    g_cancellable_cancel(cancel);
		    g_object_unref(cancel);

		    return G_SOURCE_REMOVE;
	    },
	    cancel
	);
	incrementRefcount();
	g_output_stream_write_all_async(
	    d_stream,
	    next.data(),
	    next.size(),
	    G_PRIORITY_DEFAULT,
	    cancel,
	    [](GObject *source, GAsyncResult *result, gpointer userData) -> void {
		    auto self = reinterpret_cast<Connection *>(userData);
		    Defer {
			    self->decrementRefcount();
		    };
		    auto logger =
		        self->d_logger.With(slog::Int("txID", self->d_txID.load()));

#ifndef NDEBUG
		    logger.DDebug("writeCallback");
		    Defer {
			    logger.DDebug("writeCallback done");
		    };
#endif

		    GError  *error = nullptr;
		    gsize    written;
		    gboolean ok = g_output_stream_write_all_finish(
		        G_OUTPUT_STREAM(source),
		        result,
		        &written,
		        &error
		    );

		    if (ok == false) {
			    logger.Error(
			        "write failed",
			        slog::String(
			            "error",
			            error == nullptr ? "unknown" : error->message
			        )
			    );
			    if (error != nullptr) {
				    g_error_free(error);
			    }
			    self->closeConnection();
			    self->scheduleReconnect();
			    self->scheduleDispatch();
			    return;
		    }
		    logger.DDebug("written", slog::Int("bytes", written));
		    self->d_state.store(State::CONNECTED);
		    // we reschedule a dispatch to either push next write or
		    // continue the closing.
		    self->scheduleDispatch();
	    },
	    this
	);
}

inline std::string serialize(const google::protobuf::MessageLite &m) {
	std::ostringstream oss;
	google::protobuf::util::SerializeDelimitedToOstream(m, &oss);
	return oss.str();
}

bool Connection::PostMessage(const google::protobuf::MessageLite &m) {
	const auto state = d_state.load();
	if (d_closing.load() == true || state == State::CLOSED) {
		slog::Warn(
		    "discarding as connection is closed",
		    slog::String("message", m.Utf8DebugString())
		);
		return false;
	}
	if (d_queue.try_enqueue(serialize(m)) == false) {
		slog::Error(
		    "discarding as input queue is full",
		    slog::String("message", m.Utf8DebugString())
		);
		return false;
	}
	scheduleDispatch();
	return true;
}

void Connection::scheduleDispatch() {
	d_logger.DDebug("scheduling dispatch");
	incrementRefcount();
	g_main_context_invoke(d_context, Connection::mainLoopDispatchCb, this);
}

void Connection::startClosing() {
	d_closing.store(true);
	scheduleDispatch();
}

void Connection::waitClosed() {
	atomic_wait_for_value(d_state, State::CLOSED);
}

#ifndef NDEBUG
#define DEBUG_REFCOUNT 1
#endif

void Connection::incrementRefcount() {
	auto cur = d_refCount.fetch_add(1) + 1;
#ifdef DEBUG_REFCOUNT
	auto stack = cpptrace::stacktrace::current(1);

	d_logger.Debug(
	    "incremented refcount",
	    slog::Int("refCount", cur),
	    slog::String("caller", stack.frames.front().to_string())
	);
#endif
}

void Connection::decrementRefcount() {
	auto cur = d_refCount.fetch_add(-1) - 1;
	d_refCount.notify_all();
#ifdef DEBUG_REFCOUNT
	auto stack = cpptrace::stacktrace::current(1);
	d_logger.Debug(
	    "decremented refcount",
	    slog::Int("refCount", cur),
	    slog::String("caller", stack.frames.front().to_string())
	);
#endif
}

} // namespace artemis
} // namespace fort
