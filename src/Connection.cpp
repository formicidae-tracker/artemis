#include "Connection.hpp"

#include <fort/utils/Defer.hpp>
#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>

#include <google/protobuf/message_lite.h>
#include <google/protobuf/util/delimited_message_util.h>
#include <slog++/slog++.hpp>
#include <sstream>

namespace fort {
namespace artemis {

Connection::~Connection() {
	Close();
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
	scheduleDisplatch();
}

void Connection::Close() {
	startClosing();
	waitClosed();
}

gboolean Connection::mainLoopDispatchCb(void *self_) {
	auto self = reinterpret_cast<Connection *>(self_);
	self->mainLoopDispatch();
	return G_SOURCE_REMOVE;
}

void Connection::closeConnection() {
	if (d_stream != nullptr) {
		g_output_stream_close(d_stream, nullptr, nullptr);
		g_clear_object(&d_stream);
	}
	if (d_connection != nullptr) {
		g_clear_object(&d_connection);
	}
	if (d_client != nullptr) {
		g_clear_object(&d_client);
	}
}

void Connection::mainLoopDispatch() {
	switch (d_state.load()) {
	case State::INITIAL:
		connectAsync();
		break;
	case State::CONNECTING:
	case State::WRITING:
	case State::CLOSED:
		break;
	case State::CLOSING:
		if (d_stream == nullptr) {
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
		// we send nextbuffer in case of opened connection and closing.
	case State::CONNECTED:
		sendNextBuffer();
		break;
	}
}

void Connection::connectAsync() {
	if (d_state.load() != State::INITIAL) {
		return;
	}

	if (d_client != nullptr) {
		d_client = g_socket_client_new();
	}
	d_state.store(State::CONNECTING);
	slog::Info("connecting");
	g_socket_client_connect_to_host_async(
	    d_client,
	    d_host.c_str(),
	    d_port,
	    nullptr,
	    connectCallback,
	    this
	);
}

void Connection::connectCallback(
    GObject *source, GAsyncResult *res, gpointer data
) {
	auto self = reinterpret_cast<Connection *>(data);

	GError            *error      = nullptr;
	GSocketConnection *connection = G_SOCKET_CONNECTION(
	    g_socket_client_connect_finish(G_SOCKET_CLIENT(source), res, &error)
	);

	if (connection == nullptr) {
		self->d_logger.Error(
		    "connection failed",
		    slog::String("error", error == nullptr ? "unknown" : error->message)
		);
		if (error != nullptr) {
			g_error_free(error);
		}
		self->scheduleReconnect();
		return;
	}

	if (self->d_connection != nullptr) {
		g_clear_object(&self->d_connection);
	}
	self->d_connection = connection;
	if (self->d_stream != nullptr) {
		g_clear_object(&self->d_stream);
	}
	self->d_stream = g_io_stream_get_output_stream(G_IO_STREAM(connection));
	self->d_state.store(State::CONNECTED);
	self->sendNextBuffer();
}

void Connection::scheduleReconnect() {
	d_state.store(State::CONNECTING);
	g_timeout_add(
	    guint(d_reconnectPeriod.Milliseconds()),
	    [](gpointer userData) -> gboolean {
		    auto self = reinterpret_cast<Connection *>(userData);
		    self->d_state.store(State::INITIAL);
		    self->connectAsync();
		    return G_SOURCE_REMOVE;
	    },
	    this
	);
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
	g_output_stream_write_async(
	    d_stream,
	    next.data(),
	    next.size(),
	    G_PRIORITY_DEFAULT,
	    nullptr,
	    [](GObject *source, GAsyncResult *result, gpointer userData) -> void {
		    auto    self    = reinterpret_cast<Connection *>(userData);
		    GError *error   = nullptr;
		    gssize  written = g_output_stream_write_finish(
                G_OUTPUT_STREAM(source),
                result,
                &error
            );
		    if (written < 0) {
			    self->d_logger.Error(
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
		    }
		    self->d_state.store(State::CONNECTED);
		    self->sendNextBuffer();
	    },
	    this
	);
}

inline std::string serialize(const google::protobuf::MessageLite &m) {
		std::ostringstream oss;
		google::protobuf::util::SerializeDelimitedToOstream(m, &oss);
		return oss.str();
}

void Connection::PostMessage(const google::protobuf::MessageLite &m) {
		const auto state = d_state.load();
		if (state == State::CLOSING || state == State::CLOSED) {
			slog::Warn(
			    "discarding as connection is closed",
			    slog::String("message", m.Utf8DebugString())
			);
		}
		if (d_queue.try_enqueue(serialize(m)) == false) {
			slog::Error(
			    "discarding as input queue is full",
			    slog::String("message", m.Utf8DebugString())
			);
			return;
		}
		scheduleDisplatch();
}

void Connection::scheduleDisplatch() {
		g_main_context_invoke(d_context, Connection::mainLoopDispatchCb, this);
}

void Connection::startClosing() {
		d_state.store(State::CLOSING);
		scheduleDisplatch();
}

void Connection::waitClosed() {
		d_state.wait(State::CLOSED);
}

} // namespace artemis
} // namespace fort
