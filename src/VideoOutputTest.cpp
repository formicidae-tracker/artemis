#include "VideoOutput.hpp"
#include "ImageU8.hpp"
#include "Options.hpp"
#include "utils/StringManipulation.hpp"
#include "utils/exec.hpp"

#include <atomic>
#include <glib.h>

#include "gmock/gmock.h"

#include <chrono>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <sstream>
#include <sys/wait.h>
#include <thread>

#include <gtest/gtest.h>

#include <cpptrace/exceptions.hpp>
using namespace std::chrono_literals;

namespace fort {
namespace artemis {

class VideoOutputTest : public ::testing::Test {
protected:
	static std::filesystem::path s_output;
	std::string                  d_dockerContainerID;
	GMainLoop                   *d_loop;
	std::thread                  d_mainLoopThread;

	VideoOutputOptions d_options;

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
		setenv(
		    "GST_DEBUG",
		    "GST_CAPS:6,appsrc:6,videoconvert:6,x264enc:6,*:2",
		    true
		);
	}

	static void TearDownTestSuite() {
		if (s_output.empty() ||
		    std::getenv("ARTEMIS_TESTS_VIDEO_KEEP") != nullptr) {
			return;
		}
		std::filesystem::remove_all(s_output);
	}

	void SetUp() {
		for (auto entry : std::filesystem::directory_iterator{s_output}) {
			if (entry.is_regular_file() == false ||
			    entry.path().extension() != ".mp4") {
				continue;
			}
			std::filesystem::remove(entry);
		}

		d_loop = g_main_loop_new(nullptr, FALSE);
		std::atomic<bool> ready{false};
		g_timeout_add(
		    1,
		    [](gpointer udata) {
			    auto ready = reinterpret_cast<std::atomic<bool> *>(udata);
			    ready->store(true);

			    ready->notify_all();
			    return G_SOURCE_REMOVE;
		    },
		    &ready
		);
		d_mainLoopThread = std::thread([this]() {
			g_main_loop_run(d_loop);
			g_main_loop_unref(d_loop);
		});
		ready.wait(false);
		d_options.OutputDir    = s_output;
		d_options.Height       = 360;
		d_options.StreamHeight = 240;
	}

	bool StartRtscpServer(bool assert = false) {
		if (d_dockerContainerID.empty() == false) {
			throw cpptrace::runtime_error{"docker container started"};
		}
		try {
			auto [output, res] = ExecAndCapture(
			    "docker",
			    "run",
			    "--rm",
			    "-d",
			    "-p",
			    "127.0.0.1:1935:1935",
			    "bluenviron/mediamtx:1"
			);
			if (res != 0 || output.empty()) {
				if (assert == true) {
					ADD_FAILURE()
					    << "Could not start docker image : " << output;
				}
				return false;
			}
			d_dockerContainerID = base::TrimSpaces(output);
			slog::Info(
			    "docker container",
			    slog::String("ID", d_dockerContainerID)
			);
		} catch (const std::exception &e) {
			if (assert == true) {
				ADD_FAILURE() << "Could not start docker image: " << e.what();
			}
			return false;
		}
		return true;
	}

	void StopRtscpServer() {
		if (d_dockerContainerID.empty()) {
			return;
		}

		try {
			auto [output, res] =
			    ExecAndCapture("docker", "stop", d_dockerContainerID.c_str());
			if (res == 0) {
				d_dockerContainerID = "";
			}
		} catch (const std::exception &e) {
			ADD_FAILURE() << "Could not stop docker container: " << e.what();
		}
	}

	void TearDown() {
		StopRtscpServer();
		g_main_loop_quit(d_loop);
		d_mainLoopThread.join();
	}

	void
	WithTimeout(std::chrono::milliseconds timeout, std::function<void()> &&fn) {
		auto future = std::async(std::launch::async, fn);
		if (future.wait_for(timeout) == std::future_status::timeout) {
			ADD_FAILURE() << "Test Timeouted";
			new std::future<void>(std::move(future)); // Intentional leak.
		}
	}
};

std::filesystem::path VideoOutputTest::s_output;
using namespace std::chrono_literals;

TEST_F(VideoOutputTest, BuildsAndReachEOS) {
	WithTimeout(500ms, [this]() {
		VideoOutput output(
		    d_options,
		    {.FPS = 10.0, .InputResolution = {640, 480}}
		);
	});
}

class MockFrame : public Frame {
public:
	MockFrame(const ImageU8 &img, uint64_t ID, const fort::Time &time)
	    : d_image{img}
	    , d_ID{ID}
	    , d_time{time} {}

	void *Data() override {
		return d_image.buffer;
	}

	size_t Width() const override {
		return d_image.width;
	}

	size_t Height() const override {
		return d_image.height;
	}

	uint64_t Timestamp() const override {
		return d_time.MonotonicValue() / 1000;
	}

	uint64_t ID() const override {
		return d_ID;
	}

	ImageU8 ToImageU8() override {
		return d_image;
	}

private:
	ImageU8    d_image;
	uint64_t   d_ID;
	fort::Time d_time;
};

std::string readFileContent(const std::filesystem::path &filepath) {
	std::ifstream file{filepath};
	if (file.is_open() == false) {
		throw cpptrace::runtime_error{
		    "could not open '" + filepath.string() + "'"
		};
	}
	return {
	    std::istreambuf_iterator<char>{file},
	    std::istreambuf_iterator<char>{}
	};
}

TEST_F(VideoOutputTest, EncodesMultipleFilesWithMetadata) {

	WithTimeout(2500ms, [this]() {
		VideoOutput output(
		    d_options,
		    {
		        .FilePeriod      = 3 * Duration::Second,
		        .FPS             = 10.0,
		        .InputResolution = {640, 480},
		        .LeakyPush       = false,
		    }
		);
		auto img = ImageU8::OwnedPtr{new ImageU8{
		    640,
		    480,
		    (uint8_t *)malloc(640 * 480 * sizeof(uint8_t)),
		    640
		}};
		memset(img->buffer, 127, img->NeededSize());

		auto buildFrame = [&img](uint64_t ID) {
			static auto start = Time::Now();
			return std::make_shared<MockFrame>(
			    ImageU8{*img},
			    ID,
			    start.Add(ID * 100 * Duration::Millisecond)
			);
		};

		for (size_t i = 0; i < 100; ++i) {
			output.PushFrame(buildFrame(i));
		}
	});
	std::vector<std::string> videofiles;
	std::vector<std::string> metadata;
	for (auto entry : std::filesystem::directory_iterator{s_output}) {
		if (entry.is_regular_file() && entry.path().extension() == ".mp4") {
			videofiles.push_back(entry.path().string());
		}
		if (entry.is_regular_file() &&
		    entry.path().stem().stem().extension() == ".frame-matching") {
			metadata.push_back(entry.path().string());
		}
	}
	using ::testing::Property;
	using ::testing::UnorderedElementsAre;
	using namespace std::filesystem;
	ASSERT_THAT(
	    videofiles,
	    UnorderedElementsAre(
	        Property(&path::filename, "stream.0000.mp4"),
	        Property(&path::filename, "stream.0001.mp4"),
	        Property(&path::filename, "stream.0002.mp4"),
	        Property(&path::filename, "stream.0003.mp4")
	    )
	);
	ASSERT_THAT(
	    metadata,
	    UnorderedElementsAre(
	        Property(&path::filename, "stream.frame-matching.0000.txt"),
	        Property(&path::filename, "stream.frame-matching.0001.txt"),
	        Property(&path::filename, "stream.frame-matching.0002.txt"),
	        Property(&path::filename, "stream.frame-matching.0003.txt")
	    )
	);

	std::ostringstream expectedFileContent[4];
	for (int i = 0; i < 30; ++i) {
		expectedFileContent[0] << i << " " << i << std::endl;
		expectedFileContent[1] << i << " " << i + 30 << std::endl;
		expectedFileContent[2] << i << " " << i + 60 << std::endl;
		if (i < 10) {
			expectedFileContent[3] << i << " " << i + 90 << std::endl;
		}
	}

	EXPECT_EQ(
	    readFileContent(s_output / "stream.frame-matching.0000.txt"),
	    expectedFileContent[0].str()
	);

	EXPECT_EQ(
	    readFileContent(s_output / "stream.frame-matching.0001.txt"),
	    expectedFileContent[1].str()
	);

	EXPECT_EQ(
	    readFileContent(s_output / "stream.frame-matching.0002.txt"),
	    expectedFileContent[2].str()
	);

	EXPECT_EQ(
	    readFileContent(s_output / "stream.frame-matching.0003.txt"),
	    expectedFileContent[3].str()
	);
}

TEST_F(VideoOutputTest, ServerConnection) {
	if (StartRtscpServer() == false) {
		GTEST_SKIP() << "Skipped test as I cannot start the RTSCP server";
	}
}

} // namespace artemis
} // namespace fort
