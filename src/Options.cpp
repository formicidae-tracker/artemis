#include "Options.hpp"
#include "fort/time/Time.hpp"

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
	parser.AddFlag("stub-image-paths", stubImagePaths, "Use a suite of stub images instead of an actual framegrabber");
	parser.AddFlag("test-mode",TestMode,"Test mode, adds an overlay detection drawing and statistics");
	parser.AddFlag("legacy-mode",LegacyMode,"Uses a legacy mode data output for ants cataloging and video output display. The data will be convertible to the data expected by the former Keller's group tracking system");
}

cv::Size VideoOutputOptions::WorkingResolution(const cv::Size & input) const {
	return cv::Size(input.width * double(Height) / double(input.height),Height);
}

std::string formatDuration(const fort::Duration & d ) {
	std::ostringstream oss;
	oss << d;
	return oss.str();
}

ProcessOptions::ProcessOptions()
	: FrameStride(1)
	, FrameID()
	, UUID()
	, NewAntOutputDir()
	, NewAntROISize(600)
	, ImageRenewPeriod(2 * Duration::Hour) {
	d_imageRenewPeriod = formatDuration(ImageRenewPeriod);
}

void ProcessOptions::PopulateParser(options::FlagParser & parser) {
	parser.AddFlag("frame-stride",FrameStride,"Frame sequence length");
	parser.AddFlag("frame-ids",d_frameIDs,"Frame ID to consider in the frame sequence, if empty consider all");
	parser.AddFlag("new-ant-output-dir",NewAntOutputDir,"Path where to save new detected ant pictures");
	parser.AddFlag("new-ant-roi-size", NewAntROISize, "Size of the image to save when a new ant is found");
	parser.AddFlag("image-renew-period", d_imageRenewPeriod, "ant cataloguing and full frame export renew period");
	parser.AddFlag("uuid", UUID,"The UUID to mark data sent over network");
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
		throw std::invalid_argument("Image renew period (" + formatDuration(Process.ImageRenewPeriod) + ") is too small for production of large dataset (minimum: 15m)");
	}
#endif



}

} // namespace artemis
} // namespace fort
