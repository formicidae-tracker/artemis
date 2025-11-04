#include "Options.hpp"

#include <cstdint>
#include <fort/tags/fort-tags.hpp>
#include <sstream>
#include <stdexcept>

#include <fort/options/Options.hpp>
#include <fort/time/Time.hpp>

#include <utils/StringManipulation.hpp>

namespace fort {
namespace artemis {

std::vector<uint32_t> ParseCommaSeparatedListHexa(std::string &list) {
	if (list.empty()) {
		return {};
	}
	std::vector<uint32_t>    res;
	std::vector<std::string> tagIDs;
	base::SplitString(
	    list.cbegin(),
	    list.cend(),
	    ",",
	    std::back_inserter<std::vector<std::string>>(tagIDs)
	);
	res.reserve(tagIDs.size());
	for (auto tagIDStr : tagIDs) {
		if (base::HasPrefix(base::TrimSpaces(tagIDStr), "0x") == false) {
			throw std::runtime_error(
			    "'" + tagIDStr + "' is not an hexadecimal number"
			);
		}
		std::istringstream is(base::TrimSpaces(tagIDStr));
		uint32_t           tagID;
		is >> std::hex >> tagID;
		if (!is.good() && is.eof() == false) {
			std::ostringstream os;
			os << "Cannot parse '" << tagIDStr << "'  in  '" << list << "'";
			throw std::runtime_error(os.str());
		}
		res.push_back(tagID);
	}
	return res;
}

std::vector<size_t> ParseCommaSeparatedList(std::string &list) {
	if (list.empty()) {
		return {};
	}
	std::vector<size_t>      res;
	std::vector<std::string> IDs;
	base::SplitString(
	    list.cbegin(),
	    list.cend(),
	    ",",
	    std::back_inserter<std::vector<std::string>>(IDs)
	);
	res.reserve(IDs.size());
	for (auto IDstr : IDs) {
		std::istringstream is(base::TrimSpaces(IDstr));
		uint64_t           ID;
		is >> ID;
		if (!is.good() && is.eof() == false) {
			std::ostringstream os;
			os << "Cannot parse '" << IDstr << "'  in  '" << list << "'";
			throw std::runtime_error(os.str());
		}
		res.push_back(ID);
	}
	return res;
}

fort::tags::Family ParseTagFamily(const std::string &f) {
	static std::map<std::string, fort::tags::Family> families = {
	    {"", fort::tags::Family::Undefined},
	    {"16h5", fort::tags::Family::Tag16h5},
	    {"25h9", fort::tags::Family::Tag25h9},
	    {"36h10", fort::tags::Family::Tag36h10},
	    {"36h11", fort::tags::Family::Tag36h11},
	    {"36ARTag", fort::tags::Family::Tag36ARTag},
	    {"Circle21h7", fort::tags::Family::Circle21h7},
	    {"Circle49h12", fort::tags::Family::Circle49h12},
	    {"Custom48h12", fort::tags::Family::Custom48h12},
	    {"Standard41h12", fort::tags::Family::Standard41h12},
	    {"Standard52h13", fort::tags::Family::Standard52h13},
	};
	auto fi = families.find(f);
	if (fi == families.end()) {
		throw std::out_of_range("Umknown family '" + f + "'");
	}
	return fi->second;
}

std::vector<std::string> Options::StubImagePaths() const {
	std::vector<std::string> res;
	base::SplitString(
	    stubImagePaths.cbegin(),
	    stubImagePaths.cend(),
	    ",",
	    std::back_inserter<std::vector<std::string>>(res)
	);
	return res;
}

std::set<uint32_t> DisplayOptions::Highlighted() const {
	auto asVector = ParseCommaSeparatedListHexa(highlighted);
	return {asVector.begin(), asVector.end()};
}

fort::tags::Family ApriltagOptions::Family() const {
	return ParseTagFamily(family);
}

std::set<uint64_t> ProcessOptions::FrameIDs() const {
	auto IDs = ParseCommaSeparatedList(frameIDs);
	return {IDs.begin(), IDs.end()};
}

Size VideoOutputOptions::WorkingResolution(
    const std::tuple<size_t, size_t> &inputResolution
) const {
	auto [iWidth, iHeight] = inputResolution;
	return Size{int(iWidth * double(Height) / double(iHeight)), int(Height)};
}

void Options::Validate() {
	if (Process.FrameStride == 0) {
		Process.FrameStride = 1;
	}

	if (Process.frameIDs.empty()) {
		for (size_t i = 0; i < Process.FrameStride; ++i) {
			if (i > 0) {
				Process.frameIDs += ",";
			}
			Process.frameIDs += std::to_string(i);
		}
	}

	for (const auto &frameID : Process.FrameIDs()) {
		if (frameID >= Process.FrameStride) {
			throw std::invalid_argument(
			    std::to_string(frameID) +
			    " is outside of frame stride range [0;" +
			    std::to_string(Process.FrameStride) + "["
			);
		}
	}

#ifdef NDEBUG
	auto formatDuration = [](const Duration &d) {
		std::ostringstream oss;
		oss << d;
		return oss.str();
	};

	if (ImageRenewPeriod < 15 * Duration::Minute) {
		throw std::invalid_argument(
		    "Image renew period (" + formatDuration(ImageRenewPeriod) +
		    ") is too small for production of large dataset (minimum:15m)"
		);
	}
#endif
}

} // namespace artemis
} // namespace fort

std::istream &operator>>(std::istream &in, fort::Duration &value) {
	std::string str;
	in >> str;
	value = fort::Duration::Parse(str);
	return in;
}
