#include "utils/SignalTraceHandler.hpp"
#include <gmock/gmock.h>
#include <slog++/Level.hpp>
#include <slog++/slog++.hpp>

#include <glib.h>

int main(int argc, char **argv) {
	fort::artemis::InstallSignalSafeHandlers(argc, argv, true);
	::testing::InitGoogleMock(&argc, argv);

	// Make criticals fatal (errors are already fatal by default)
	g_log_set_always_fatal(static_cast<GLogLevelFlags>(
	    G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL
	    // | G_LOG_LEVEL_WARNING  // uncomment if you also want warnings to
	    // abort
	));

	slog::DefaultLogger().From(slog::Level::Debug);

	return RUN_ALL_TESTS();
}
