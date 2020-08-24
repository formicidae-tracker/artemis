#include "OptionsUTest.hpp"


#include "Options.hpp"

#include <functional>

namespace fort {
namespace artemis {

TEST_F(OptionsUTest,DefaultValues) {
	Options options;
	EXPECT_EQ(options.General.PrintHelp,false);
	EXPECT_EQ(options.General.PrintVersion,false);
	EXPECT_EQ(options.General.PrintResolution,false);
	EXPECT_EQ(options.General.LogDir,"");
	EXPECT_EQ(options.General.StubImagePath,"");
	EXPECT_EQ(options.General.TestMode,false);
	EXPECT_EQ(options.General.LegacyMode,false);

	EXPECT_TRUE(options.Display.Highlighted.empty());
	EXPECT_FALSE(options.Display.DisplayROI);


	EXPECT_EQ(options.Network.Host,"");
	EXPECT_EQ(options.Network.Port,3002);

	EXPECT_EQ(options.VideoOutput.Height,1080);
	EXPECT_EQ(options.VideoOutput.AddHeader,false);
	EXPECT_EQ(options.VideoOutput.ToStdout,false);

	EXPECT_EQ(options.Apriltag.Family,fort::tags::Family::Undefined);
	EXPECT_FLOAT_EQ(options.Apriltag.QuadDecimate,1.0);
	EXPECT_FLOAT_EQ(options.Apriltag.QuadSigma,0.0);
	EXPECT_EQ(options.Apriltag.RefineEdges,false);
	EXPECT_EQ(options.Apriltag.QuadMinClusterPixel,5);
	EXPECT_EQ(options.Apriltag.QuadMaxNMaxima,10);
	EXPECT_FLOAT_EQ(options.Apriltag.QuadCriticalRadian,0.174533);
	EXPECT_FLOAT_EQ(options.Apriltag.QuadMaxLineMSE,10.0);
	EXPECT_EQ(options.Apriltag.QuadMinBWDiff,40);
	EXPECT_EQ(options.Apriltag.QuadDeglitch,false);

	EXPECT_FLOAT_EQ(options.Camera.FPS,8.0);
	EXPECT_EQ(options.Camera.StrobeDuration,1500 * Duration::Microsecond);
	EXPECT_EQ(options.Camera.StrobeDelay,0);

	EXPECT_EQ(options.Process.FrameStride,1);
	EXPECT_TRUE(options.Process.FrameID.empty());
	EXPECT_EQ(options.Process.NewAntOutputDir,"");
	EXPECT_EQ(options.Process.NewAntROISize,600);
	EXPECT_EQ(options.Process.ImageRenewPeriod,2 * Duration::Hour);
	EXPECT_EQ(options.Process.UUID,"");

}

TEST_F(OptionsUTest,TestParse) {
	struct TestData{
		std::vector<const char*>                   Args;
		std::function<void(const Options &)> Test;
	};

	std::vector<TestData> testData
		= {
		   {{"artemis","--version"},
		    [](const Options & options) {
			    EXPECT_TRUE(options.General.PrintVersion);
		    }},
		   {{"artemis","--help"},
		    [](const Options & options) {
			    EXPECT_TRUE(options.General.PrintHelp);
		    }},
		   {{"artemis","--fetch-resolution"},
		    [](const Options & options) {
			    EXPECT_TRUE(options.General.PrintResolution);
		    }},
		   {{"artemis","--log-output-dir","foo"},
		    [](const Options & options) {
			    EXPECT_EQ(options.General.LogDir,"foo");
		    }},
		   {{"artemis","--stub-image-path","foo"},
		    [](const Options & options) {
			    EXPECT_EQ(options.General.StubImagePath,"foo");
		    }},
		   {{"artemis","--test-mode"},
		    [](const Options & options) {
			    EXPECT_TRUE(options.General.TestMode);
		    }},
		   {{"artemis","--legacy-mode"},
		    [](const Options & options) {
			    EXPECT_TRUE(options.General.LegacyMode);
		    }},
		   {{"artemis","--host", "foo", "--at-family", "36h11"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Network.Host,"foo");
		    }},
		   {{"artemis","--port", "1234"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Network.Port,1234);
		    }},
		   {{"artemis","--uuid", "abcdef123456"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Process.UUID,"abcdef123456");
		    }},
		   {{"artemis","--video-output-to-stdout"},
		    [](const Options & options) {
			    EXPECT_TRUE(options.VideoOutput.ToStdout);
		    }},
		   {{"artemis","--video-output-add-header"},
		    [](const Options & options) {
			    EXPECT_TRUE(options.VideoOutput.AddHeader);
		    }},
		   {{"artemis","--video-output-height", "1200"},
		    [](const Options & options) {
			    EXPECT_EQ(options.VideoOutput.Height,1200);
		    }},
		   {{"artemis","--highlight-tags", "0x001,0x0ae"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Display.Highlighted.size(),2);
			    if ( options.Display.Highlighted.size() >= 2 ) {
				    EXPECT_EQ(options.Display.Highlighted[0],1);
				    EXPECT_EQ(options.Display.Highlighted[1],0x0ae);
			    }
		    }},

		   {{"artemis","--display-roi"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Display.DisplayROI,true);
		    }},

		   {{"artemis","--frame-stride", "33"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Process.FrameStride,33);
		    }},
		   // frame strid eis need for validation
		   {{"artemis","--frame-stride", "33", "--frame-ids", "1,2,3"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Process.FrameID.size(),3);
			    if ( options.Process.FrameID.size() >= 3 ) {
				    auto it = options.Process.FrameID.begin();
				    EXPECT_EQ(*it,1);
				    EXPECT_EQ(*(++it),2);
				    EXPECT_EQ(*(++it),3);
			    }
		    }},

		   {{"artemis","--new-ant-output-dir", "foo"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Process.NewAntOutputDir,"foo");
		    }},

		   {{"artemis","--new-ant-roi-size", "600"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Process.NewAntROISize,600);
		    }},

		   {{"artemis","--image-renew-period", "3h42m12s"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Process.ImageRenewPeriod,3 * Duration::Hour + 42 * Duration::Minute + 12 * Duration::Second);
		    }},

		   {{"artemis","--camera-fps", "12.45"},
		    [](const Options & options) {
			    EXPECT_DOUBLE_EQ(options.Camera.FPS,12.45);
		    }},

		   {{"artemis","--camera-strobe-us", "1245us"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Camera.StrobeDuration,1245 * Duration::Microsecond);
		    }},

		   {{"artemis","--camera-strobe-delay-us", "-12us"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Camera.StrobeDelay,-12 * Duration::Microsecond);
		    }},

		   {{"artemis","--camera-slave-width", "7920"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Camera.SlaveWidth,7920);
		    }},

		   {{"artemis","--camera-slave-height", "6004"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Camera.SlaveHeight,6004);
		    }},

		   {{"artemis","--at-quad-decimate", "1.5"},
		    [](const Options & options) {
			    EXPECT_FLOAT_EQ(options.Apriltag.QuadDecimate,1.5);
		    }},

		   {{"artemis","--at-quad-sigma", "1.1"},
		    [](const Options & options) {
			    EXPECT_FLOAT_EQ(options.Apriltag.QuadSigma,1.1);
		    }},

		   {{"artemis","--at-refine-edges"},
		    [](const Options & options) {
			    EXPECT_TRUE(options.Apriltag.RefineEdges);
		    }},

		   {{"artemis","--at-quad-min-cluster", "56"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Apriltag.QuadMinClusterPixel,56);
		    }},

		   {{"artemis","--at-quad-max-n-maxima", "56"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Apriltag.QuadMaxNMaxima,56);
		    }},

		   {{"artemis","--at-quad-critical-radian", "0.1"},
		    [](const Options & options) {
			    EXPECT_FLOAT_EQ(options.Apriltag.QuadCriticalRadian,0.1);
		    }},

		   {{"artemis","--at-quad-max-line-mse", "0.1"},
		    [](const Options & options) {
			    EXPECT_FLOAT_EQ(options.Apriltag.QuadMaxLineMSE,0.1);
		    }},

		   {{"artemis","--at-quad-min-bw-diff", "56"},
		    [](const Options & options) {
			    EXPECT_EQ(options.Apriltag.QuadMinBWDiff,56);
		    }},

		   {{"artemis","--at-quad-deglitch"},
		    [](const Options & options) {
			    EXPECT_TRUE(options.Apriltag.QuadDeglitch);
		    }},

		   {{"artemis","--at-family", "36ARTag"},
		    [](const Options & options) {
		   	    EXPECT_EQ(options.Apriltag.Family,fort::tags::Family::Tag36ARTag);
		    }},
	};



	for( const auto & d : testData ) {
		int nArgs = d.Args.size();
		try {
			d.Test(Options::Parse(nArgs,(char**)&(d.Args[0])));
		} catch ( const std::exception & e ) {

			ADD_FAILURE() << "Unexpected exception: " << e.what();
		}


	}
}

TEST_F(OptionsUTest,TestValidation) {
	Options options;
	options.Process.FrameStride = 0;
	EXPECT_NO_THROW({
			options.Validate();
		});
	EXPECT_EQ(options.Process.FrameStride,1);
	EXPECT_EQ(options.Process.FrameID.size(),1);
	EXPECT_EQ(options.Process.FrameID.count(0),1);
	options.Process.FrameID.clear();
	options.Process.FrameStride = 3;
	options.Validate();
	EXPECT_EQ(options.Process.FrameID.size(),3);
	EXPECT_EQ(options.Process.FrameID.count(0),1);
	EXPECT_EQ(options.Process.FrameID.count(1),1);
	EXPECT_EQ(options.Process.FrameID.count(2),1);
	EXPECT_NO_THROW({options.Validate();});
	options.Process.FrameID.insert(3);
	try {
		options.Validate();
		ADD_FAILURE() << "Should have thrown an invalid argument";
	} catch ( const std::invalid_argument &  e ) {
		EXPECT_STREQ(e.what(),"3 is outside of frame stride range [0;3[");
	}
#ifdef NDEBUG
	options.Process.FrameID.clear();
	options.Process.ImageRenewPeriod = 1 *  Duration::Second;
	try {
		options.Validate();
		ADD_FAILURE() << "Validation should have thrown";
	} catch (  const std::invalid_argument & e ) {
		EXPECT_STREQ(e.what(),"Image renew period (1s) is too small for production of large dataset (minimum: 15m)");

	}
#endif
}

} // namespace artemis
} // namespace fort
