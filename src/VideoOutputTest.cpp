#include "VideoOutput.hpp"
#include "Options.hpp"
#include <cstdlib>
#include <filesystem>
#include <future>
#include <glib.h>
#include <gtest/gtest.h>

namespace fort {
namespace artemis {

class VideoOutputTest : public ::testing::Test {
protected:
	static std::filesystem::path s_output;
	GMainLoop                   *d_loop;
	std::thread                  d_mainLoopThread;

	static void SetUpTestSuite() {
		char tempdir[] = "/tmp/artemis-video-test-XXXXXX";
		auto res       = mkdtemp(tempdir);
		if (res == nullptr) {
			FAIL() << "Could not create tempdir: " << errno;
			s_output = "";
			return;
		}
		s_output = res;
		slog::Info("output dir", slog::String("path", s_output));
	}

	static void TearDownTestSuite() {
		if (s_output.empty()) {
			return;
		}

		std::filesystem::remove_all(s_output);
	}

	void SetUp() {
		d_mainLoopThread = std::thread([this]() {
			d_loop = g_main_loop_new(nullptr, FALSE);
			Defer {
				g_main_loop_unref(d_loop);
			};
			g_main_loop_run(d_loop);
		});
	}

	void TearDown() {
		g_main_loop_quit(d_loop);
		d_mainLoopThread.join();
	}
};

std::filesystem::path VideoOutputTest::s_output;

TEST_F(VideoOutputTest, BuildsAndReachEOS) {
	VideoOutputOptions options;
	options.OutputDir = s_output;

	auto future = std::async(std::launch::async, [&options]() {
		VideoOutput output(options, {1920, 1080}, 10.0);
	});

	if (future.wait_for(std::chrono::milliseconds{500}) ==
	    std::future_status::timeout) {
		ADD_FAILURE() << "Test timeouted";
		new std::future<void>(std::move(future)); // intentional leak
	}
}

} // namespace artemis
} // namespace fort
