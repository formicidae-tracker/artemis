#include "Options.hpp"

#include <stdexcept>


#include <utils/FlagParser.hpp>
#include <utils/StringManipulation.hpp>

namespace fort {
namespace artemis {

std::vector<uint32_t> ParseCommaSeparatedListHexa(std::string & list) {
	if ( list.empty() ) {
		return {};
	}
	std::vector<uint32_t> res;
	std::vector<std::string> tagIDs;
	base::SplitString(list.cbegin(),
	                  list.cend(),
	                  ",",
	                  std::back_inserter<std::vector<std::string>>(tagIDs));
	res.reserve(tagIDs.size());
	for (auto tagIDStr : tagIDs) {
		if ( base::HasPrefix(base::TrimSpaces(tagIDStr),"0x") == false ) {
			throw std::runtime_error("'" + tagIDStr + "' is not an hexadecimal number");
		}
		std::istringstream is(base::TrimSpaces(tagIDStr));
		uint32_t tagID;
		is >> std::hex >> tagID;
		if ( !is.good() && is.eof() == false ) {
			std::ostringstream os;
			os << "Cannot parse '" << tagIDStr << "'  in  '" << list << "'";
			throw std::runtime_error(os.str());
		}
		res.push_back(tagID);
	}
	return res;
}

std::vector<size_t> ParseCommaSeparatedList(std::string & list) {
	if ( list.empty() ) {
		return {};
	}
	std::vector<size_t> res;
	std::vector<std::string> IDs;
	base::SplitString(list.cbegin(),
	                  list.cend(),
	                  ",",
	                  std::back_inserter<std::vector<std::string>>(IDs));
	res.reserve(IDs.size());
	for (auto IDstr : IDs) {
		std::istringstream is(base::TrimSpaces(IDstr));
		uint64_t ID;
		is >> ID;
		if ( !is.good() && is.eof() == false ) {
			std::ostringstream os;
			os << "Cannot parse '" << IDstr << "'  in  '" << list << "'";
			throw std::runtime_error(os.str());
		}
		res.push_back(ID);
	}
	return res;
}

fort::tags::Family ParseTagFamily(const std::string & f) {
	static std::map<std::string,fort::tags::Family> families
		= {
		   {"",fort::tags::Family::Undefined},
		   {"16h5",fort::tags::Family::Tag16h5},
		   {"25h9",fort::tags::Family::Tag25h9},
		   {"36h10",fort::tags::Family::Tag36h10},
		   {"36h11",fort::tags::Family::Tag36h11},
		   {"36ARTag",fort::tags::Family::Tag36ARTag},
		   {"Circle21h7",fort::tags::Family::Circle21h7},
		   {"Circle49h12",fort::tags::Family::Circle49h12},
		   {"Custom48h12",fort::tags::Family::Custom48h12},
		   {"Standard41h12",fort::tags::Family::Standard41h12},
		   {"Standard52h13",fort::tags::Family::Standard52h13},
	};
	auto fi = families.find(f);
	if ( fi == families.end() ) {
		throw std::out_of_range("Umknown family '" + f + "'");
	}
	return fi->second;
}



GeneralOptions::GeneralOptions()
	: PrintHelp(false)
	, PrintVersion(false)
	, PrintResolution(false)
	, LogDir("")
	, TestMode(false)
	, LegacyMode(false) {
}

void GeneralOptions::PopulateParser(options::FlagParser & parser) {
	parser.AddFlag("help",PrintHelp,"Print this help message",'h');
	parser.AddFlag("fetch-resolution",PrintResolution,"Print the camera resolution");
	parser.AddFlag("version",PrintVersion,"Print version");
	parser.AddFlag("log-output-dir",LogDir,"Directory to put logs in");
	parser.AddFlag("stub-image-path", StubImagePath, "Use a stub image instead of an actual framegrabber");
	parser.AddFlag("test-mode",TestMode,"Test mode, adds an overlay detection drawing and statistics");
	parser.AddFlag("legacy-mode",LegacyMode,"Uses a legacy mode data output for ants cataloging and video output display. The data will be convertible to the data expected by the former Keller's group tracking system");
}

void GeneralOptions::FinishParse() {}


NetworkOptions::NetworkOptions()
	: Host()
	, Port(3002) {
}

void NetworkOptions::PopulateParser(options::FlagParser & parser) {
	parser.AddFlag("host", Host, "Host to send tag detection readout");
	parser.AddFlag("port", Port, "Port to send tag detection readout",'p');
}

void NetworkOptions::FinishParse() {}

VideoOutputOptions::VideoOutputOptions()
	: Height(1080)
	, AddHeader(false)
	, ToStdout(false) {
}

void VideoOutputOptions::PopulateParser(options::FlagParser & parser) {
	parser.AddFlag("video-output-to-stdout", ToStdout, "Sends video output to stdout");
	parser.AddFlag("video-output-height", Height, "Video Output height (width computed to maintain aspect ratio");
	parser.AddFlag("video-output-add-header", AddHeader, "Adds binary header to stdout output");
}

void VideoOutputOptions::FinishParse() {
}

cv::Size VideoOutputOptions::WorkingResolution(const cv::Size & input) const {
	return cv::Size(input.width * double(Height) / double(input.height),Height);
}

DisplayOptions::DisplayOptions()
	: DisplayROI(false) {
}

void DisplayOptions::PopulateParser(options::FlagParser & parser) {
		parser.AddFlag("highlight-tags",d_highlighted,"Tag to highlight when drawing detections");
		parser.AddFlag("display-roi",DisplayROI,"Displays ROI");
}

void DisplayOptions::FinishParse() {
	Highlighted = ParseCommaSeparatedListHexa(d_highlighted);
}


ProcessOptions::ProcessOptions()
	: FrameStride(1)
	, FrameID()
	, UUID()
	, NewAntOutputDir()
	, NewAntROISize(600)
	, ImageRenewPeriod(2 * Duration::Hour) {
	d_imageRenewPeriod = ImageRenewPeriod.ToString();
}

void ProcessOptions::PopulateParser(options::FlagParser & parser) {
	parser.AddFlag("frame-stride",FrameStride,"Frame sequence length");
	parser.AddFlag("frame-ids",d_frameIDs,"Frame ID to consider in the frame sequence, if empty consider all");
	parser.AddFlag("new-ant-output-dir",NewAntOutputDir,"Path where to save new detected ant pictures");
	parser.AddFlag("new-ant-roi-size", NewAntROISize, "Size of the image to save when a new ant is found");
	parser.AddFlag("image-renew-period", d_imageRenewPeriod, "ant cataloguing and full frame export renew period");
	parser.AddFlag("uuid", UUID,"The UUID to mark data sent over network");
}

void ProcessOptions::FinishParse() {
	auto IDs = ParseCommaSeparatedList(d_frameIDs);
	FrameID.clear();
	FrameID.insert(IDs.begin(),IDs.end());
	ImageRenewPeriod = Duration::Parse(d_imageRenewPeriod);
}

CameraOptions::CameraOptions()
	: FPS(8.0)
	, StrobeDuration(1500 * Duration::Microsecond)
	, StrobeDelay(0) {
	d_strobeDuration = StrobeDuration.ToString();
	d_strobeDelay = StrobeDelay.ToString();
}

void CameraOptions::PopulateParser(options::FlagParser & parser)  {
	parser.AddFlag("camera-fps",FPS,"Camera FPS to use");
	parser.AddFlag("camera-slave-width",SlaveWidth,"Camera Width argument for slave mode");
	parser.AddFlag("camera-slave-height",SlaveHeight,"Camera Height argument for slave mode");
	parser.AddFlag("camera-strobe-us",d_strobeDuration,"Camera Strobe Length in us");
	parser.AddFlag("camera-strobe-delay-us",d_strobeDelay,"Camera Strobe Delay in us, negative value allowed");
}

void CameraOptions::FinishParse() {
	StrobeDuration = Duration::Parse(d_strobeDuration);
	StrobeDelay = Duration::Parse(d_strobeDelay);
}

ApriltagOptions::ApriltagOptions()
	: Family(fort::tags::Family::Undefined)
	, QuadDecimate(1.0)
	, QuadSigma(0.0)
	, RefineEdges(false)
	, QuadMinClusterPixel(5)
	, QuadMaxNMaxima(10)
	, QuadCriticalRadian(0.174533)
	, QuadMaxLineMSE(10.0)
	, QuadMinBWDiff(40)
	, QuadDeglitch(false) {
 }

void ApriltagOptions::PopulateParser(options::FlagParser & parser)  {
	parser.AddFlag("at-quad-decimate",QuadDecimate,"Decimate original image for faster computation but worse pose estimation. Should be 1.0 (no decimation), 1.5, 2, 3 or 4");
	parser.AddFlag("at-quad-sigma",QuadSigma,"Apply a gaussian filter for quad detection, noisy image likes a slight filter like 0.8, for ant detection, 0.0 is almost always fine");
	parser.AddFlag("at-refine-edges",RefineEdges,"Refines the edge of the quad, especially needed if decimation is used.");
	parser.AddFlag("at-quad-min-cluster",QuadMinClusterPixel,"Minimum number of pixel to consider it a quad");
	parser.AddFlag("at-quad-max-n-maxima",QuadMaxNMaxima,"Number of candidate to consider to fit quad corner");
	parser.AddFlag("at-quad-critical-radian",QuadCriticalRadian,"Rejects quad with angle to close to 0 or 180 degrees");
	parser.AddFlag("at-quad-max-line-mse",QuadMaxLineMSE,"MSE threshold to reject a fitted quad");
	parser.AddFlag("at-quad-min-bw-diff",QuadMinBWDiff,"Difference in pixel value to consider a region black or white");
	parser.AddFlag("at-quad-deglitch",QuadDeglitch,"Deglitch only for noisy images");
	parser.AddFlag("at-family",d_family,"The apriltag family to use");
}

void ApriltagOptions::FinishParse() {
	Family = ParseTagFamily(d_family);
}


void Options::PopulateParser(options::FlagParser & parser)  {
	General.PopulateParser(parser);
	Display.PopulateParser(parser);
	Network.PopulateParser(parser);
	VideoOutput.PopulateParser(parser);
	Apriltag.PopulateParser(parser);
	Camera.PopulateParser(parser);
	Process.PopulateParser(parser);
}

void Options::FinishParse()  {
	General.FinishParse();
	Display.FinishParse();
	Network.FinishParse();
	VideoOutput.FinishParse();
	Apriltag.FinishParse();
	Camera.FinishParse();
	Process.FinishParse();
}

Options Options::Parse(int & argc, char ** argv, bool printHelp) {
	Options opts;
	options::FlagParser parser(options::FlagParser::Default,"low-level vision detection for the FORmicidae Tracker");

	opts.PopulateParser(parser);
	parser.Parse(argc,argv);
	opts.FinishParse();

	if ( opts.General.PrintHelp && printHelp ) {
		parser.PrintUsage(std::cerr);
		exit(0);
	}

	opts.Validate();


	return opts;
}


void Options::Validate() {

	if ( Process.FrameStride == 0 ) {
		Process.FrameStride = 1;
	}

	if ( Process.FrameID.empty() ) {
		for ( size_t i = 0; i < Process.FrameStride; ++i ) {
			Process.FrameID.insert(i);
		}
	}


	for ( const auto & frameID : Process.FrameID ) {
		if ( frameID >= Process.FrameStride ) {
			throw std::invalid_argument(std::to_string(frameID)
			                            + " is outside of frame stride range [0;"
			                            + std::to_string(Process.FrameStride) + "[");

		}
	}
#ifdef NDEBUG
	if ( Process.ImageRenewPeriod < 15 * Duration::Minute ) {
		throw std::invalid_argument("Image renew period (" + Process.ImageRenewPeriod.ToString() + ") is too small for production of large dataset (minimum: 15m)");
	}
#endif


	if ( Network.Host.empty() == false
	     && Apriltag.Family == fort::tags::Family::Undefined ) {
		throw std::runtime_error("Connection to '"
		                         + Network.Host
		                         + ":"
		                         + std::to_string(Network.Port)
		                         + " requested but no family selected");
	}

	if ( General.TestMode ) {
		Display.DisplayROI = true;
	}



}

} // namespace artemis
} // namespace fort
