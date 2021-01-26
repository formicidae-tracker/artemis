#include "UserInterface.hpp"

#include "../utils/StringManipulation.hpp"

namespace fort {
namespace artemis {

UserInterface::UserInterface(const cv::Size & workingResolution,
                             const Options & options,
                             const ROIChannelPtr & roiChannel)
	: d_roiChannel(roiChannel)
	, d_highlighted(options.Display.Highlighted.begin(),
	                options.Display.Highlighted.end())
	, d_watermark(options.General.TestMode ? "TEST MODE" : "")
	, d_displayROI(options.General.TestMode == true)
	, d_displayLabels(false)
	, d_displayHelp(false)
	, d_displayOverlay(true)  {

}

UserInterface::~UserInterface(){}

void UserInterface::ToggleHighlight(uint32_t tagID) {
	if ( d_highlighted.count(tagID) == 0 ) {
		d_highlighted.insert(tagID);
	} else {
		d_highlighted.erase(tagID);
	}
}



void UserInterface::ROIChanged(const cv::Rect & roi) {
	d_roiChannel->push(roi);
}

UserInterface::DataToDisplay
UserInterface::ComputeDataToDisplay(const std::shared_ptr<hermes::FrameReadout> & m ) {
	DataToDisplay data;
	data.HighlightedIndexes.clear();
	data.NormalIndexes.clear();
	if ( !m ) {
		return data;
	}
	data.HighlightedIndexes.reserve(m->tags_size());
	data.NormalIndexes.reserve(m->tags_size());
	size_t idx = -1;
	for ( const auto & t : m->tags() ) {
		++idx;
		if ( d_highlighted.count(t.id()) != 0 ) {
			data.HighlightedIndexes.push_back(idx);
		} else {
			data.NormalIndexes.push_back(idx);
		}
	}
	return data;
}

void UserInterface::PushFrame(const FrameToDisplay & frame) {
	UpdateFrame(frame,ComputeDataToDisplay(frame.Message));
}


void UserInterface::ToggleDisplayROI()  {
	d_displayROI = !d_displayROI;
}

void UserInterface::ToggleDisplayLabels() {
	d_displayLabels = !d_displayLabels;
}

void UserInterface::ToggleDisplayHelp() {
	d_displayHelp = !d_displayHelp;
}

void UserInterface::ToggleDisplayOverlay() {
	d_displayOverlay = !d_displayOverlay;
}

bool UserInterface::DisplayROI() const {
	return d_prompt.empty() && d_displayROI;
}

bool UserInterface::DisplayLabels() const {
	return d_prompt.empty()
		&& d_displayROI == false
		&& d_displayHelp == false
		&& d_displayLabels;
}

bool UserInterface::DisplayHelp() const {
	return d_prompt.empty() && d_displayHelp;
}

bool UserInterface::DisplayOverlay() const {
	return d_prompt.empty() && d_displayHelp == false && d_displayOverlay;
}

const std::string & UserInterface::Watermark() const {
	return d_watermark;
}

std::string UserInterface::PromptAndValue() const {
	return d_prompt + d_value;
}

const std::string & UserInterface::Value() const {
	return d_value;
}


void UserInterface::EnterHighlightPrompt() {
	d_value.clear();
	std::ostringstream prompt;
	if ( d_highlighted.empty() ) {
		prompt << "No highlighted tags" << std::endl;
	} else {
		prompt << "highlighted tags:" << std::endl;
		for ( const auto & t : d_highlighted ) {
			prompt << " * 0x" << std::hex << std::setfill('0') << std::setw(3) << t << std::endl;
		}
	}
	prompt << "Toggle Highlighting for TagID : ";
	d_prompt = prompt.str();
}

void UserInterface::LeaveHighlightPrompt() {
	base::TrimSpaces(d_value);
	if ( base::HasPrefix(d_value,"0x") == true ) {

		std::istringstream iss(d_value);
		uint32_t h;
		iss >> std::hex >> h;
		if ( iss.fail() == false ) {
			ToggleHighlight(h);
		}
	}
	d_value.clear();
	d_prompt.clear();
}

void UserInterface::AppendPromptValue( char c ) {
	if ( c  == '\n' ) {
		LeaveHighlightPrompt();
	}
	d_value += c;
}



} // namespace artemis
} // namespace fort
