#include <cpptrace/from_current.hpp>

#include "OptionsUTest.hpp"

#include "Options.hpp"

#include "gtest/gtest.h"
#include <functional>

namespace fort {
namespace artemis {

TEST_F(OptionsUTest, DefaultValues) {
	Options options;
	EXPECT_EQ(options.PrintVersion, false);
	EXPECT_EQ(options.PrintResolution, false);
	EXPECT_EQ(options.LogDir, "");
	EXPECT_TRUE(options.StubImagePaths().empty());
	EXPECT_EQ(options.TestMode, false);
	EXPECT_EQ(options.LegacyMode, false);

	EXPECT_TRUE(options.Display.Highlighted().empty());

	EXPECT_EQ(options.Leto.Host, "");
	EXPECT_EQ(options.Leto.Port, 3002);

	EXPECT_EQ(options.VideoOutput.Height, 1080);
	EXPECT_EQ(options.VideoOutput.Height, 1080);
	EXPECT_EQ(options.VideoOutput.StreamHeight, 1080);
	EXPECT_EQ(options.VideoOutput.OutputDir, "");
	EXPECT_EQ(options.VideoOutput.Host, "");
	EXPECT_EQ(options.VideoOutput.Quality, "fast");
	EXPECT_EQ(options.VideoOutput.Tune, "film");
	EXPECT_EQ(options.VideoOutput.Bitrate_KB, 2000);
	EXPECT_FLOAT_EQ(options.VideoOutput.BitrateMaxRatio, 1.5);

	EXPECT_EQ(options.Apriltag.Family(), fort::tags::Family::Undefined);
	EXPECT_FLOAT_EQ(options.Apriltag.QuadDecimate, 1.0);
	EXPECT_FLOAT_EQ(options.Apriltag.QuadSigma, 0.0);
	EXPECT_EQ(options.Apriltag.RefineEdges, false);
	EXPECT_EQ(options.Apriltag.QuadMinClusterPixel, 5);
	EXPECT_EQ(options.Apriltag.QuadMaxNMaxima, 10);
	EXPECT_FLOAT_EQ(options.Apriltag.QuadCriticalRadian, 0.174533);
	EXPECT_FLOAT_EQ(options.Apriltag.QuadMaxLineMSE, 10.0);
	EXPECT_EQ(options.Apriltag.QuadMinBWDiff, 40);
	EXPECT_EQ(options.Apriltag.QuadDeglitch, false);

	EXPECT_FLOAT_EQ(options.Camera.FPS, 8.0);
	EXPECT_EQ(options.Camera.StrobeDuration, 1500 * Duration::Microsecond);
	EXPECT_EQ(options.Camera.StrobeDelay, 0);

	EXPECT_EQ(options.Process.FrameStride, 1);
	EXPECT_TRUE(options.Process.FrameIDs().empty());
	EXPECT_EQ(options.NewAntOutputDir, "");
	EXPECT_EQ(options.NewAntROISize, 600);
	EXPECT_EQ(options.ImageRenewPeriod, 2 * Duration::Hour);
	EXPECT_EQ(options.Process.UUID, "");
}

TEST_F(OptionsUTest, TestParse) {
	struct TestData {
		std::vector<const char *>            Args;
		std::function<void(const Options &)> Test;
	};

	std::vector<TestData> testData = {
	    {{"artemis", "--version"},
	     [](const Options &options) { EXPECT_TRUE(options.PrintVersion); }},
	    {{"artemis", "--fetch-resolution"},
	     [](const Options &options) { EXPECT_TRUE(options.PrintResolution); }},
	    {{"artemis", "--log-output-dir", "foo"},
	     [](const Options &options) { EXPECT_EQ(options.LogDir, "foo"); }},
	    {{"artemis", "--stub-image-paths", "foo,bar,baz"},
	     [](const Options &options) {
		     const auto paths = options.StubImagePaths();
		     EXPECT_EQ(paths.size(), 3);
		     if (paths.size() == 3) {
			     EXPECT_EQ(paths[0], "foo");
			     EXPECT_EQ(paths[1], "bar");
			     EXPECT_EQ(paths[2], "baz");
		     }
	     }},
	    {{"artemis", "--test-mode"},
	     [](const Options &options) { EXPECT_TRUE(options.TestMode); }},
	    {{"artemis", "--legacy-mode"},
	     [](const Options &options) { EXPECT_TRUE(options.LegacyMode); }},
	    {{"artemis", "--leto.host", "foo", "--at.family", "36h11"},
	     [](const Options &options) { EXPECT_EQ(options.Leto.Host, "foo"); }},
	    {{"artemis", "--leto.port", "1234"},
	     [](const Options &options) { EXPECT_EQ(options.Leto.Port, 1234); }},
	    {{"artemis", "--process.uuid", "abcdef123456"},
	     [](const Options &options) {
		     EXPECT_EQ(options.Process.UUID, "abcdef123456");
	     }},
	    {{"artemis", "--video-output.dir", "foo"},
	     [](const Options &options) {
		     EXPECT_EQ(options.VideoOutput.OutputDir, "foo");
	     }},
	    {{"artemis", "--video-output.host", "bar"},
	     [](const Options &options) {
		     EXPECT_EQ(options.VideoOutput.Host, "bar");
	     }},
	    {{"artemis", "--video-output.height", "2160"},
	     [](const Options &options) {
		     EXPECT_EQ(options.VideoOutput.Height, 2160);
	     }},
	    {{"artemis", "--video-output.stream-height", "2160"},
	     [](const Options &options) {
		     EXPECT_EQ(options.VideoOutput.StreamHeight, 2160);
	     }},
	    {{"artemis", "--video-output.bitrate", "1200"},
	     [](const Options &options) {
		     EXPECT_EQ(options.VideoOutput.Bitrate_KB, 1200);
	     }},
	    {{"artemis", "--video-output.bitrate-max-ratio", "2.0"},
	     [](const Options &options) {
		     EXPECT_EQ(options.VideoOutput.BitrateMaxRatio, 2.0);
	     }},
	    {{"artemis", "--video-output.quality", "foo"},
	     [](const Options &options) {
		     EXPECT_EQ(options.VideoOutput.Quality, "foo");
	     }},
	    {{"artemis", "--video-output.tune", "tune"},
	     [](const Options &options) {
		     EXPECT_EQ(options.VideoOutput.Tune, "tune");
	     }},
	    {{"artemis", "--display.highlight-tags", "0x001,0x0ae"},
	     [](const Options &options) {
		     const auto highlighted = options.Display.Highlighted();
		     EXPECT_EQ(highlighted.size(), 2);
		     EXPECT_EQ(highlighted.count(1), 1);
		     EXPECT_EQ(highlighted.count(0x0ae), 1);
	     }},

	    {{"artemis", "--process.stride", "33"},
	     [](const Options &options) {
		     EXPECT_EQ(options.Process.FrameStride, 33);
	     }},
	    // frame strid eis need for validation
	    {{"artemis", "--process.stride", "33", "--process.ids", "1,2,3"},
	     [](const Options &options) {
		     auto frameIDs = options.Process.FrameIDs();
		     EXPECT_EQ(frameIDs.size(), 3);
		     if (frameIDs.size() >= 3) {
			     auto it = frameIDs.begin();
			     EXPECT_EQ(*it, 1);
			     EXPECT_EQ(*(++it), 2);
			     EXPECT_EQ(*(++it), 3);
		     }
	     }},

	    {{"artemis", "--new-ant-output-dir", "foo"},
	     [](const Options &options) {
		     EXPECT_EQ(options.NewAntOutputDir, "foo");
	     }},

	    {{"artemis", "--new-ant-roi-size", "600"},
	     [](const Options &options) { EXPECT_EQ(options.NewAntROISize, 600); }},

	    {{"artemis", "--renew-period", "3h42m12s"},
	     [](const Options &options) {
		     EXPECT_EQ(
		         options.ImageRenewPeriod,
		         3 * Duration::Hour + 42 * Duration::Minute +
		             12 * Duration::Second
		     );
	     }},

	    {{"artemis", "--camera.fps", "12.45"},
	     [](const Options &options) {
		     EXPECT_DOUBLE_EQ(options.Camera.FPS, 12.45);
	     }},

	    {{"artemis", "--camera.strobe", "1245us"},
	     [](const Options &options) {
		     EXPECT_EQ(
		         options.Camera.StrobeDuration,
		         1245 * Duration::Microsecond
		     );
	     }},

	    {{"artemis", "--camera.strobe-delay=-12us"},
	     [](const Options &options) {
		     EXPECT_EQ(options.Camera.StrobeDelay, -12 * Duration::Microsecond);
	     }},

	    {{"artemis", "--camera.slave-width", "7920"},
	     [](const Options &options) {
		     EXPECT_EQ(options.Camera.SlaveWidth, 7920);
	     }},

	    {{"artemis", "--camera.slave-height", "6004"},
	     [](const Options &options) {
		     EXPECT_EQ(options.Camera.SlaveHeight, 6004);
	     }},

	    {{"artemis", "--at.quad-decimate", "1.5"},
	     [](const Options &options) {
		     EXPECT_FLOAT_EQ(options.Apriltag.QuadDecimate, 1.5);
	     }},

	    {{"artemis", "--at.quad-sigma", "1.1"},
	     [](const Options &options) {
		     EXPECT_FLOAT_EQ(options.Apriltag.QuadSigma, 1.1);
	     }},

	    {{"artemis", "--at.refine-edges"},
	     [](const Options &options) {
		     EXPECT_TRUE(options.Apriltag.RefineEdges);
	     }},

	    {{"artemis", "--at.quad-min-cluster", "56"},
	     [](const Options &options) {
		     EXPECT_EQ(options.Apriltag.QuadMinClusterPixel, 56);
	     }},

	    {{"artemis", "--at.quad-max-n-maxima", "56"},
	     [](const Options &options) {
		     EXPECT_EQ(options.Apriltag.QuadMaxNMaxima, 56);
	     }},

	    {{"artemis", "--at.quad-critical-radian", "0.1"},
	     [](const Options &options) {
		     EXPECT_FLOAT_EQ(options.Apriltag.QuadCriticalRadian, 0.1);
	     }},

	    {{"artemis", "--at.quad-max-line-mse", "0.1"},
	     [](const Options &options) {
		     EXPECT_FLOAT_EQ(options.Apriltag.QuadMaxLineMSE, 0.1);
	     }},

	    {{"artemis", "--at.quad-min-bw-diff", "56"},
	     [](const Options &options) {
		     EXPECT_EQ(options.Apriltag.QuadMinBWDiff, 56);
	     }},

	    {{"artemis", "--at.quad-deglitch"},
	     [](const Options &options) {
		     EXPECT_TRUE(options.Apriltag.QuadDeglitch);
	     }},

	    {{"artemis", "--at.family", "36ARTag"},
	     [](const Options &options) {
		     EXPECT_EQ(
		         options.Apriltag.Family(),
		         fort::tags::Family::Tag36ARTag
		     );
	     }},
	};

	for (auto &d : testData) {
		int nArgs = d.Args.size();
		try {
			Options opts;
			opts.ParseArguments(nArgs, &(d.Args[0]));
			d.Test(opts);
		} catch (const std::exception &e) {
			ADD_FAILURE() << "Unexpected exception: " << e.what() << std::endl
			              << "when parsing " << d.Args[1];
			cpptrace::from_current_exception().print();
		}
	}

	auto helpCall = []() {
		Options     opts;
		int         argc    = 2;
		const char *argv[2] = {"artemis", "--help"};
		opts.ParseArguments(argc, argv);
	};

	EXPECT_EXIT(helpCall(), ::testing::ExitedWithCode(0), "Usage:");
}

TEST_F(OptionsUTest, TestValidation) {
	Options options;
	options.Process.FrameStride = 0;
	EXPECT_NO_THROW({ options.Validate(); });
	EXPECT_EQ(options.Process.FrameStride, 1);
	auto frameIDs = options.Process.FrameIDs();
	EXPECT_EQ(frameIDs.size(), 1);
	EXPECT_EQ(frameIDs.count(0), 1);
	options.Process.frameIDs.clear();
	options.Process.FrameStride = 3;
	options.Validate();
	frameIDs = options.Process.FrameIDs();
	EXPECT_EQ(frameIDs.size(), 3);
	EXPECT_EQ(frameIDs.count(0), 1);
	EXPECT_EQ(frameIDs.count(1), 1);
	EXPECT_EQ(frameIDs.count(2), 1);
	EXPECT_NO_THROW({ options.Validate(); });

	options.Process.frameIDs = "0,1,2,3";
	try {
		options.Validate();
		ADD_FAILURE() << "Should have thrown an invalid argument";
	} catch (const std::invalid_argument &e) {
		EXPECT_STREQ(e.what(), "3 is outside of frame stride range [0;3[");
	}
#ifdef NDEBUG
	options.Process.frameIDs.clear();
	options.ImageRenewPeriod = 1 * Duration::Second;
	try {
		options.Validate();
		ADD_FAILURE() << "Validation should have thrown";
	} catch (const std::invalid_argument &e) {
		EXPECT_STREQ(
		    e.what(),
		    "Image renew period (1s) is too small for production of large "
		    "dataset (minimum:15m)"
		);
	}
#endif
}

} // namespace artemis
} // namespace fort
