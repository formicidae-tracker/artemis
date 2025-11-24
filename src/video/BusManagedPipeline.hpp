#pragma once

#include "gstreamer.hpp"
#include <gst/gstelement.h>
#include <gst/gstmessage.h>

#include <atomic>
#include <filesystem>

#include <slog++/slog++.hpp>

namespace fort {
namespace artemis {
class BusManagedPipeline {
public:
	BusManagedPipeline(const std::string &name);
	BusManagedPipeline(const std::string &name, const std::string &description);
	virtual ~BusManagedPipeline();

	// disable copy and move.
	BusManagedPipeline(const BusManagedPipeline &)            = delete;
	BusManagedPipeline(BusManagedPipeline &&)                 = delete;
	BusManagedPipeline &operator=(const BusManagedPipeline &) = delete;
	BusManagedPipeline &operator=(BusManagedPipeline &&)      = delete;

	GstStateChangeReturn SetState(GstState state);

	GstElementPtr GetByName(const std::string &name) const;

	inline GstElement *Self() const {
		return d_pipeline.get();
	}

	inline GstBin *Bin() const {
		return GST_BIN(d_pipeline.get());
	}

	void PrintDebug(const std::filesystem::path &filepath) const;

protected:
	virtual void OnMessage(GstBus *bus, GstMessage *message);
	void         logGstMessage(GstMessage *message);

	slog::Logger<1> d_logger;

	void waitOnEOS();

private:
	void init();

	void onMessage(GstBus *bus, GstMessage *message);

	GstElementPtr     d_pipeline;
	GstBusPtr         d_bus;
	std::string       d_name;
	guint             d_watch{0};
	std::atomic<bool> d_eosReached{false};
};

} // namespace artemis
} // namespace fort
