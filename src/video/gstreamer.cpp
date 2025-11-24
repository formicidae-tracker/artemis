#include "gstreamer.hpp"

#include <mutex>

namespace fort {
namespace artemis {
static std::once_flag gst_initialized;

void EnsureGSTInitialized() {
	std::call_once(gst_initialized, []() {
		int    argc = 0;
		char **argv = {nullptr};
		gst_init(&argc, &argv);
	});
}

} // namespace artemis
} // namespace fort
