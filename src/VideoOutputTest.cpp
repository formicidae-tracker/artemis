#include "VideoOutput.hpp"
#include "ImageU8.hpp"
#include "Options.hpp"
#include "StubFrameGrabber.hpp"
#include <chrono>
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
	}

	static void TearDownTestSuite() {
		if (s_output.empty() ||
		    std::getenv("ARTEMIS_TESTS_VIDEO_KEEP") != nullptr) {
			return;
		}
		std::filesystem::remove_all(s_output);
	}

	void SetUp() {
		d_mainLoopThread          = std::thread([this]() {
            d_loop = g_main_loop_new(nullptr, FALSE);
            Defer {
                g_main_loop_unref(d_loop);
            };
            g_main_loop_run(d_loop);
        });
		d_options.OutputDir       = s_output;
		d_options.Height          = 360;
		d_options.StreamHeight    = 240;
		d_options.SegmentDuration = Duration::Second;
	}

	void TearDown() {
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
		VideoOutput output(d_options, {640, 480}, 10.0);
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

TEST_F(VideoOutputTest, EncodesMultipleImages) {
	WithTimeout(2500ms, [this]() {
		VideoOutput output(d_options, {640, 480}, 10.0);
		auto        img = ImageU8::OwnedPtr{new ImageU8{
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
	ADD_FAILURE() << "coucou";
}

} // namespace artemis
} // namespace fort
