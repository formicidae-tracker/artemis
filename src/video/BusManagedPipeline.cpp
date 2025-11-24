#include <glib-object.h>
#include <glib.h>
#include <gst/app/gstappsrc.h>
#include <gst/gstbus.h>
#include <gst/gstelement.h>
#include <gst/gstenumtypes.h>
#include <gst/gstmessage.h>
#include <gst/gstpipeline.h>

#include <chrono>
#include <fstream>
#include <memory>

#include <cpptrace/exceptions.hpp>

#include <fort/utils/Defer.hpp>

#include <slog++/slog++.hpp>

#include "BusManagedPipeline.hpp"
#include "video/gstreamer.hpp"

using namespace std::chrono_literals;

namespace fort {
namespace artemis {

BusManagedPipeline::BusManagedPipeline(const std::string &name)
    : d_logger{slog::With(slog::String("video_pipeline", name))} {
	EnsureGSTInitialized();
	d_pipeline = GstElementPtr{gst_pipeline_new(name.c_str())};
	if (d_pipeline == nullptr) {
		throw cpptrace::logic_error{"could not build pipeline '" + name + "'"};
	}
	init();
}

BusManagedPipeline::BusManagedPipeline(
    const std::string &name, const std::string &description
)
    : d_logger{slog::With(slog::String("video_pipeline", name))} {

	EnsureGSTInitialized();

	d_logger.Debug("pipeline", slog::String("description", description));

	GError *error = nullptr;
	d_pipeline = GstElementPtr{gst_parse_launch(description.c_str(), &error)};
	if (error != nullptr) {
		d_pipeline.reset();
		Defer {
			g_error_free(error);
		};

		throw cpptrace::runtime_error{
		    "could not create pipeline '" + description +
		    "': " + (error->message == nullptr ? "<unknown>" : error->message)
		};
	}
	gst_element_set_name(Self(), name.c_str());

	init();
}

void BusManagedPipeline::init() {
	d_name   = gst_element_get_name(Self());
	d_logger = slog::With(slog::String("video_pipeline", d_name));
	d_bus    = GstBusPtr{gst_element_get_bus(d_pipeline.get())};
	d_watch  = gst_bus_add_watch(
        d_bus.get(),
        [](GstBus *bus, GstMessage *message, gpointer userdata) -> gboolean {
            auto self = reinterpret_cast<BusManagedPipeline *>(userdata);
            self->onMessage(bus, message);
            return G_SOURCE_CONTINUE;
        },
        this
    );
}

void BusManagedPipeline::waitOnEOS() {
	while (d_eosReached.load() == false) {

		GstState state, pending;
		auto     change = gst_element_get_state(
            Self(),
            &state,
            &pending,
            std::chrono::nanoseconds{10ms}.count()
        );

		if (state == GST_STATE_NULL) {
			continue;
		}
		if (state != GST_STATE_PLAYING || change == GST_STATE_CHANGE_FAILURE) {
			d_logger.Error(
			    "Pipeline is not playing on closing. Data maybe lost.",
			    slog::String(
			        "pipeline",
			        (const char *)gst_element_get_name(Self())
			    ),
			    slog::String(
			        "state",
			        (const char *)g_enum_to_string(gst_state_get_type(), state)
			    ),
			    slog::String(
			        "pending",
			        (const char *)
			            g_enum_to_string(gst_state_get_type(), pending)
			    ),
			    slog::String(
			        "state_change",
			        (const char *)g_enum_to_string(
			            gst_state_change_return_get_type(),
			            change
			        )
			    )
			);
			break;
		}
		d_logger.DDebug(
		    "waiting on EOS",
		    slog::String(
		        "state",
		        (const char *)g_enum_to_string(gst_state_get_type(), state)
		    ),
		    slog::String(
		        "pending",
		        (const char *)g_enum_to_string(gst_state_get_type(), pending)
		    ),
		    slog::String(
		        "state_change",
		        (const char *)
		            g_enum_to_string(gst_state_change_return_get_type(), change)
		    )
		);
	}
}

BusManagedPipeline::~BusManagedPipeline() {
	struct Context {
		BusManagedPipeline *self;
		std::atomic<bool>   done{false};
		Context(BusManagedPipeline *self_)
		    : self{self_} {};
	};

	auto ctx = std::make_unique<Context>(this);
	g_main_context_invoke(
	    g_main_context_default(),
	    [](gpointer userdata) -> gboolean {
		    auto context = reinterpret_cast<Context *>(userdata);
		    Defer {
			    context->done.store(true);
			    context->done.notify_all();
		    };
		    if (context->self->d_watch == 0) {
			    return G_SOURCE_REMOVE;
		    }
		    g_source_remove(context->self->d_watch);

		    return G_SOURCE_REMOVE;
	    },
	    ctx.get()
	);

	ctx->done.wait(false);
	gst_element_set_state(Self(), GST_STATE_NULL);
}

void BusManagedPipeline::PrintDebug(const std::filesystem::path &filepath
) const {

	auto data = std::string{gst_debug_bin_to_dot_data(
	    GST_BIN(d_pipeline.get()),
	    GST_DEBUG_GRAPH_SHOW_ALL
	)};

	std::ofstream file(filepath);
	file << data;
	file.close();
	d_logger.Info(
	    "saved pipeline debug file",
	    slog::String("filepath", filepath)
	);
}

void BusManagedPipeline::OnMessage(GstBus *bus, GstMessage *message) {
	logGstMessage(message);
}

#ifndef NDEBUG
#define VIDEO_LOG_STATE_CHANGE 1
#endif

void BusManagedPipeline::onMessage(GstBus *bus, GstMessage *message) {
	auto type = GST_MESSAGE_TYPE(message);
	switch (type) {
	case GST_MESSAGE_EOS:
		d_logger.Info("End-Of-Stream reached");
		gst_element_set_state(d_pipeline.get(), GST_STATE_NULL);
		d_eosReached.store(true);
		return;
#ifdef VIDEO_LOG_STATE_CHANGE
	case GST_MESSAGE_STATE_CHANGED:
		GstState oldstate, newstate, pending;
		gst_message_parse_state_changed(
		    message,
		    &oldstate,
		    &newstate,
		    &pending
		);
		d_logger.Info(
		    "state changed",
		    slog::String("element", (const char *)message->src->name),
		    slog::String(
		        "oldstate",
		        (const char *)g_enum_to_string(gst_state_get_type(), oldstate)
		    ),
		    slog::String(
		        "newstate",
		        (const char *)g_enum_to_string(gst_state_get_type(), newstate)
		    ),
		    slog::String(
		        "pending",
		        (const char *)g_enum_to_string(gst_state_get_type(), pending)
		    )
		);
		return;
#endif
	default:
		OnMessage(bus, message);
		return;
	}
}

void BusManagedPipeline::logGstMessage(GstMessage *message) {
	GError *err{nullptr};
	gchar  *debug_info{nullptr};
	auto    level = slog::Level::Info;
	switch (GST_MESSAGE_TYPE(message)) {
	case GST_MESSAGE_INFO:
#ifdef NDEBUG
		return;
#endif
		gst_message_parse_info(message, &err, &debug_info);
		level = slog::Level::Info;
		break;
	case GST_MESSAGE_ERROR:
		level = slog::Level::Error;
		gst_message_parse_error(message, &err, &debug_info);
		break;
	case GST_MESSAGE_WARNING:
		gst_message_parse_warning(message, &err, &debug_info);
		level = slog::Level::Warn;
		break;
	default:
		d_logger.DDebug(
		    "unhandled message type",
		    slog::String(
		        "message_type",
		        (const char *)gst_message_type_get_name(GST_MESSAGE_TYPE(message
		        ))
		    )
		);
		return;
	}

	d_logger.Log(
	    level,
	    "pipeline message",
	    slog::String("source", (const char *)message->src->name),
	    slog::String("error", (const char *)err->message),
	    slog::String(
	        "debug_info",
	        debug_info == nullptr ? "<none>" : (const char *)debug_info
	    )
	);

	g_clear_error(&err);
	if (debug_info != nullptr) {
		g_free(debug_info);
	}
}

GstElementPtr BusManagedPipeline::GetByName(const std::string &name) const {
	auto element = GstElementPtr{gst_bin_get_by_name(Bin(), name.c_str())};
	if (element == nullptr) {
		throw cpptrace::runtime_error{
		    "could not find element '" + name + "' in pipeline '" + d_name + "'"
		};
	}
	return element;
}

GstStateChangeReturn BusManagedPipeline::SetState(GstState state) {
	return gst_element_set_state(Self(), state);
}
} // namespace artemis
} // namespace fort
