#pragma once

#include <opencv2/core.hpp>

#include <fort/tags/fort-tags.hpp>

#include <fort/options/Options.hpp>

namespace fort {
namespace artemis {

struct GeneralOptions : public options::Group {};

struct DisplayOptions : public options::Group {
	std::vector<uint32_t> &Highlighted = AddRepeatableOption<uint32_t>(
	    "highlight-tags", "Tag to highlight when drawing detections"
	);
};

struct NetworkOptions : public options::Group {
	std::string &Host =
	    AddOption<std::string>("host", "Host to send tag detection readout")
	        .SetDefault("");
	uint16_t &Port =
	    AddOption<uint16_t>("p,port", "port to send tag detection readout")
	        .SetDefault(3002);
};

struct VideoOutputOptions : public options::Group {
	cv::Size WorkingResolution(const cv::Size &inputResolution) const;

	bool &ToStdout =
	    AddOption<bool>("to-stdout", "Sends video output to stdout");

	size_t &Height = AddOption<size_t>(
	    "height",
	    "Video output height (width is computed to maintain aspect ratio"
	);
	bool &AddHeader = AddOption<bool>(
	    "add-header", "Adds binary header to stdout video output"
	);
};

struct ApriltagOptions : public options::Group {
	fort::tags::Family &Family =
	    AddOption<fort::tags::Family>("family", "The family tag to use")
	        .SetDefault(fort::tags::Family::Undefined);
	float &QuadDecimate =
	    AddOption<float>(
	        "quad-decimate",
	        "Decimate original image for faster computation but worse pose "
	        "estimation. Should be 1.0 (no decimation), 1.5,2,3 or 4."
	    )
	        .SetDefault(1.0);
	float &QuadSigma = AddOption<float>(
	                       "quad-sigma",
	                       "Applies a gaussian filter for quad detection, "
	                       "noisy image likes a slight filter like 0.8, for "
	                       "ant detection, 0.0 is almost always fine"
	)
	                       .SetDefault(0.0);

	bool &RefineEdges = AddOption<bool>(
	    "refine-edges",
	    "Refines the edge of the quad, especially needed if decimation is used."
	);

	int &QuadMinClusterPixel =
	    AddOption<int>(
	        "quad-min-cluster", "Minimum number of pixel to consider it a quad"
	    )
	        .SetDefault(5);

	int &QuadMaxNMaxima =
	    AddOption<int>(
	        "quad-max-n-maxima",

	        "Number of candidate to consider to fit quad corner"
	    )
	        .SetDefault(10);

	float &QuadCriticalRadian =
	    AddOption<float>(
	        "quad-critical-radian",
	        "Rejects quad with angle to close to 0 or 180 degrees."
	    )
	        .SetDefault(0.174533);

	float &QuadMaxLineMSE =
	    AddOption<float>(
	        "quad-max-line-mse", "MSE threshold to reject a fitted quad"
	    )
	        .SetDefault(10.0);
	int &QuadMinBWDiff =
	    AddOption<int>(
	        "quad-min-bw-diff",
	        "Difference in pixel value to consider a region black or white"
	    )
	        .SetDefault(40);
	bool &QuadDeglitch =
	    AddOption<bool>("quad-deglitch", "Deglitch only for noisy images");
};

struct CameraOptions : public options::Group {
	double &FPS = AddOption<double>("fps", "Camera FPS to use").SetDefault(8.0);

	Duration &StrobeDuration =
	    AddOption<Duration>("strobe", "Camera strobe duration to use")
	        .SetDefault(1500 * Duration::Microsecond);
	Duration &StrobeDelay =
	    AddOption<Duration>("delay", "Camera strobe delay to use")
	        .SetDefault(0);
	size_t &Width =
	    AddOption<size_t>("width", "Width to use for mode that requires it")
	        .SetDefault(0);
	size_t &Height =
	    AddOption<size_t>(
	        "height", "Height to use for camera without a camera setting"
	    )
	        .SetDefault(0);
};

struct ProcessOptions : public options::Group {
	size_t &FrameStride std::set<uint64_t> &FrameID std::string &UUID;

	std::string &NewAntOutputDir;
	size_t      &NewAntROISize;
	Duration    &ImageRenewPeriod;
};

struct Options {
	bool &PrintVersion =
	    AddOption<bool>("V,version", "print current version and exists");
	bool PrintResolution = AddOption<bool>(
	    "fetch-resolution", "Print the camera resolution and exists"
	);

	std::string &LogDir =
	    AddOption<std::string>("log-output-dir", "Directory to put logs in")
	        .SetDefault("");
	std::vector<std::string> &StubImagePaths = AddRepeatableOption<std::string>(
	    "stub-image-paths",
	    "A collection of stub images to use instead of camera input"
	);

	bool &TestMode =
	    AddOption<bool>("test-mode", "Adds an overlay to indicate test mode");
	bool &LegacyMode = AddOption<bool>(
	    "legacy-mode", "Uses a legacy mode for data output and ant cataloging"
	);

	DisplayOptions     Display;
	NetworkOptions     Network;
	VideoOutputOptions VideoOutput;
	ApriltagOptions    Apriltag;
	CameraOptions      Camera;
	ProcessOptions     Process;

	static Options Parse(int &argc, char **argv, bool printHelp = false);

	void Validate();

private:
	void PopulateParser(options::FlagParser &parser);
	void FinishParse();
};

} // namespace artemis
} // namespace fort
