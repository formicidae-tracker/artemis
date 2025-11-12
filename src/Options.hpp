#pragma once

#include "Rect.hpp"
#include <cstdint>
#include <istream>
#include <set>

#include <fort/time/Time.hpp>
// Istream operator should be declared before Options.
std::istream &operator>>(std::istream &in, fort::Duration &duration);

#include <fort/options/Options.hpp>
#include <fort/tags/fort-tags.hpp>

namespace fort {
namespace artemis {

struct DisplayOptions : public options::Group {
protected:
	std::string &highlighted =
	    AddOption<std::string>(
	        "highlight-tags",
	        "Comma separated list of tags to highlight when drawing detections"
	    )
	        .SetDefault("");

public:
	std::set<uint32_t> Highlighted() const;
};

struct LetoOptions : public options::Group {
	std::string &Host =
	    AddOption<std::string>("host", "Host to send tag detection readout")
	        .SetDefault("");
	uint16_t &Port =
	    AddOption<uint16_t>("port", "Host to send tag detection readout")
	        .SetDefault(3002);
};

struct VideoOutputOptions : public options::Group {
	static Size
	TargetResolution(size_t targetHeight, const Size &inputResolution);

	size_t &Height =
	    AddOption<size_t>("height", "Video output height. 0 for original size")
	        .SetDefault(1080);

	size_t &StreamHeight =
	    AddOption<size_t>("stream-height", "Video output height")
	        .SetDefault(1080);

	std::string &OutputDir =
	    AddOption<std::string>("dir", "Video output dir, disable if empty")
	        .SetDefault("");

	std::string &Host =
	    AddOption<std::string>("host", "Host to stream to, disabled if empty")
	        .SetDefault("");

	int &Bitrate_KB =
	    AddOption<int>("bitrate", "Mean bitrate in kbps for disk encoding")
	        .SetDefault(2000);

	float &BitrateMaxRatio =
	    AddOption<float>("bitrate-max-ratio", "maximum peek bitrate")
	        .SetDefault(1.5);

	std::string &Quality =
	    AddOption<std::string>("quality", "libx264 quality preset")
	        .SetDefault("fast");

	std::string &Tune = AddOption<std::string>("tune", "libx264 tune preset")
	                        .SetDefault("film");

	Duration &SegmentDuration =
	    AddOption<Duration>(
	        "segment-duration", "duration of each video segment"
	    )
	        .SetDefault(2 * Duration::Hour);
};

struct ApriltagOptions : public options::Group {
public:
	std::string &family =
	    AddOption<std::string>("family", "Family to use for detection")
	        .SetDefault("");

	tags::Family Family() const;

	float &QuadDecimate =
	    AddOption<float>(
	        "quad-decimate",
	        "Decimate original image for faster computation but worse pose "
	        "estimation. Should be 1.0 (no decimation), 1.5, 2, 3 or 4"
	    )
	        .SetDefault(1.0);
	float &QuadSigma =
	    AddOption<float>(
	        "quad-sigma",
	        "Apply a gaussian filter for quad detection, noisy image likes a "
	        "slight filter like 0.8, for ant detection, 0.0 is almost always "
	        "fine"
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
	        "Rejects quad with angle to close to 0 or 180 degrees"
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
	    AddOption<bool>("quad-deglitch", "Deglitch for noisy images");
};

struct CameraOptions : public options::Group {

	double &FPS =
	    AddOption<double>("fps", "Desired camera FPS").SetDefault(8.0);
	Duration &StrobeDuration = AddOption<Duration>("strobe", "strobe duration")
	                               .SetDefault(1500 * Duration::Microsecond);
	Duration &StrobeDelay =
	    AddOption<Duration>("strobe-delay", "Camera strobe delay")
	        .SetDefault(0);
	size_t &SlaveWidth =
	    AddOption<size_t>("slave-width", "Camera Width for slave mode")
	        .SetDefault(0);
	size_t &SlaveHeight =
	    AddOption<size_t>("slave-height", "Camera Height for slave mode")
	        .SetDefault(0);
};

struct ProcessOptions : public options::Group {
	std::string &frameIDs =
	    AddOption<std::string>(
	        "ids",
	        "Frame ID to process in the sequence, if empty, consider alls"
	    )
	        .SetDefault("");

	size_t &FrameStride =
	    AddOption<size_t>("stride", "Frame sequence length").SetDefault(1);

	std::set<uint64_t> FrameIDs() const;

	std::string &UUID = AddOption<std::string>(
	                        "uuid", "The UUID to mark data sent over network"
	)
	                        .SetDefault("");
};

struct Options : public options::Group {
protected:
	std::string &stubImagePaths = AddOption<std::string>(
	                                  "stub-image-paths",
	                                  "Stub images to use instead of the "
	                                  "camera as a comma separated list."
	)
	                                  .SetDefault("");

public:
	bool &PrintVersion =
	    AddOption<bool>("version,V", "Print the version and exit");
	bool &PrintResolution =
	    AddOption<bool>("fetch-resolution", "Prints the camera resolution");
	std::string &LogDir =
	    AddOption<std::string>("log-output-dir", "Directory to puts logs in")
	        .SetDefault("");

	bool &TestMode =
	    AddOption<bool>("test-mode", "Run with a test-mode overlay");
	bool &LegacyMode = AddOption<bool>(
	    "legacy-mode",
	    "Uses a legacy mode data output for ants cataloging and video output "
	    "display. The data will be convertible to the data expected by the "
	    "former Keller's group tracking system"
	);

	std::string &NewAntOutputDir =
	    AddOption<std::string>(
	        "new-ant-output-dir", "Path to save new detected individuals"
	    )
	        .SetDefault("");
	size_t &NewAntROISize =
	    AddOption<size_t>(
	        "new-ant-roi-size", "Size of the close-up on new individuals"
	    )
	        .SetDefault(600);
	Duration &ImageRenewPeriod =
	    AddOption<Duration>(
	        "renew-period", "Individual cataloguing and stream renew period"
	    )
	        .SetDefault(2 * Duration::Hour);

	std::vector<std::string> StubImagePaths() const;

	DisplayOptions &Display =
	    AddSubgroup<DisplayOptions>("display", "display options");
	LetoOptions &Leto = AddSubgroup<LetoOptions>(
	    "leto", "Options regarding communication with leto"
	);
	VideoOutputOptions &VideoOutput = AddSubgroup<VideoOutputOptions>(
	    "video-output", "Options regarding video output"
	);
	ApriltagOptions &Apriltag =
	    AddSubgroup<ApriltagOptions>("at", "option regarding tag detection");
	CameraOptions &Camera = AddSubgroup<CameraOptions>(
	    "camera", "options regarding camera and illumination"
	);
	ProcessOptions &Process = AddSubgroup<ProcessOptions>(
	    "process", "options regarding process of frames"
	);

	void Validate();
};

} // namespace artemis
} // namespace fort
