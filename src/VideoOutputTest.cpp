#include "VideoOutput.hpp"
#include "Options.hpp"
#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>

namespace fort {
namespace artemis {

class VideoOutputTest : public ::testing::Test {
protected:
	static std::filesystem::path s_output;

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
};

std::filesystem::path VideoOutputTest::s_output;

TEST_F(VideoOutputTest, Builds) {
	VideoOutputOptions options;
	options.OutputDir = s_output;

	VideoOutput output(options, {1920, 1080}, 10.0);
}

} // namespace artemis
} // namespace fort
