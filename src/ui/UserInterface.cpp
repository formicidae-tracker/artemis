#include "UserInterface.hpp"

namespace fort {
namespace artemis {

UserInterface::UserInterface(const cv::Size & workingResolution,
                             const DisplayOptions & options,
                             const ZoomChannelPtr & zoomChannel)
	: d_zoomChannel(zoomChannel)
	, d_highlighted(options.Highlighted.begin(),
	                options.Highlighted.end())
	,d_displayROI(options.DisplayROI) {

}

void UserInterface::ToggleHighlight(uint32_t tagID) {
	if ( d_highlighted.count(tagID) == 0 ) {
		d_highlighted.insert(tagID);
	} else {
		d_highlighted.erase(tagID);
	}
}


void UserInterface::SetROIDisplay(bool displayROI) {
	d_displayROI = displayROI;
}


void UserInterface::ZoomChanged(const Zoom & zoom) {
	d_zoomChannel->push(zoom);
}

UserInterface::DataToDisplay
UserInterface::ComputeDataToDisplay(const std::shared_ptr<hermes::FrameReadout> & m ) {
	DataToDisplay data;
	data.DisplayROI = d_displayROI;
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


} // namespace artemis
} // namespace fort
