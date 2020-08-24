#include "UserInterface.hpp"

namespace fort {
namespace artemis {

UserInterface::UserInterface(const cv::Size & workingResolution,
                             const DisplayOptions & options)
	: d_zoom({.Scale = 1.0,
	          .Center = cv::Point(workingResolution.width / 2,
	                              workingResolution.height / 2),
		})
	, d_highlighted(options.Highlighted.begin(),
	                options.Highlighted.end())
	,d_displayROI(options.DisplayROI) {

	d_onZoom = [](const Zoom & ) {};
}

void UserInterface::OnZoom(const OnZoomCallback & callback) {
	d_onZoom = callback;
}

UserInterface::Zoom UserInterface::CurrentZoom() const {
	return d_zoom;
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
	d_zoom = zoom;
	d_onZoom(zoom);
}

UserInterface::DataToDisplay
UserInterface::ComputeDataToDisplay(const std::shared_ptr<hermes::FrameReadout> & m ) {
	DataToDisplay data {.DisplayROI = d_displayROI };
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
