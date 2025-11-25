#include "gstreamer.hpp"

#include <glib.h>
#include <mutex>

#include "fort/utils/Defer.hpp"

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

std::string GEnumToString(GType type, gint value) {
	auto res = g_enum_to_string(type, value);
	Defer {
		if (res != nullptr) {
			g_free(res);
		}
	};

	return res == nullptr ? "<UNKNOWN>" : res;
}

} // namespace artemis
} // namespace fort
