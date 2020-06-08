#include "FrameDisplayer.h"

#include "DrawDetectionProcess.h"
#include "OverlayWriter.h"

#include <opencv2/highgui/highgui.hpp>

#include <glog/logging.h>

#include <csignal>

const double FrameDisplayer::MinZoom = 1.0;
const double FrameDisplayer::ZoomIncrement = 0.25;
const double FrameDisplayer::MaxZoom = 8.0;

double clamp(double v, double l, double h) {
	return std::min(std::max(v,l),h);
}

FrameDisplayer::FrameDisplayer(bool desactivateQuit,
                               const std::shared_ptr<DrawDetectionProcess> & ddProcess,
                               const std::shared_ptr<OverlayWriter> & oWriter)
	: d_initialized(false)
	, d_inhibQuit(desactivateQuit)
	, d_ddProcess(ddProcess)
	, d_oWriter(oWriter)
	, d_highlight(-1) {
	cv::namedWindow("artemis output",cv::WINDOW_NORMAL | cv::WINDOW_KEEPRATIO | cv::WINDOW_GUI_EXPANDED);
	cv::setMouseCallback("artemis output", &StaticOnMouseCallback, reinterpret_cast<void*>(this));
}

FrameDisplayer::~FrameDisplayer() {

}


cv::Rect FrameDisplayer::BuildROI(const cv::Size & size) {
	if (d_initialized == false ) {
		d_initialized = true;
		d_zoom = MinZoom;
		d_x = size.width / 2;
		d_y = size.height / 2;
		d_mouseLastX = -1;
		d_mouseLastY = -1;
	}

	cv::Size targetSize(size.width / d_zoom,size.height / d_zoom);
	int x = clamp(d_x - targetSize.width / 2, 0, size.width - targetSize.width);
	int y = clamp(d_y - targetSize.height / 2,0, size.height - targetSize.height);
	d_x = clamp(d_x,0,size.width - targetSize.width/2);
	d_y = clamp(d_y,0,size.height - targetSize.height/2);
	cv::Rect res(x, y, targetSize.width, targetSize.height);


	return res;
}


void FrameDisplayer::StaticOnMouseCallback(int event, int x, int y, int flags, void*myself) {
	reinterpret_cast<FrameDisplayer*>(myself)->OnMouseCallback(event, x,y,flags);
}

std::map<int,std::string> MouseEvent = {
                                        {cv::EVENT_MOUSEMOVE,"EVENT_MOUSEMOVE"},
                                        {cv::EVENT_LBUTTONDOWN,"EVENT_LBUTTONDOWN"},
                                        {cv::EVENT_RBUTTONDOWN,"EVENT_RBUTTONDOWN"},
                                        {cv::EVENT_MBUTTONDOWN,"EVENT_MBUTTONDOWN"},
                                        {cv::EVENT_LBUTTONUP,"EVENT_LBUTTONUP"},
                                        {cv::EVENT_RBUTTONUP,"EVENT_RBUTTONUP"},
                                        {cv::EVENT_MBUTTONUP,"EVENT_MBUTTONUP"},
                                        {cv::EVENT_LBUTTONDBLCLK,"EVENT_LBUTTONDBLCLK"},
                                        {cv::EVENT_RBUTTONDBLCLK,"EVENT_RBUTTONDBLCLK"},
                                        {cv::EVENT_MBUTTONDBLCLK,"EVENT_MBUTTONDBLCLK"},
                                        {cv::EVENT_MOUSEWHEEL,"EVENT_MOUSEWHEEL"},
                                        {cv::EVENT_MOUSEHWHEEL,"EVENT_MOUSEHWHEEL"},
};


void FrameDisplayer::OnMouseCallback(int event, int x, int y, int flags) {


	if ( (event == cv::EVENT_MOUSEWHEEL) || (event == cv::EVENT_MOUSEHWHEEL) ) {
		if (flags < 0 ) {
			d_zoom = clamp(d_zoom + ZoomIncrement,MinZoom,MaxZoom);
		} else {
			d_zoom = clamp(d_zoom - ZoomIncrement,MinZoom,MaxZoom);
		}
	}


	if ( event == cv::EVENT_LBUTTONUP ) {
		d_mouseLastX = -1;
		d_mouseLastY = -1;
	}

	if ( event == cv::EVENT_LBUTTONDOWN ) {
		d_mouseLastX = x;
		d_mouseLastY = y;
	}


	if (event == cv::EVENT_MOUSEMOVE ) {
		if ( d_mouseLastX >= 0 && d_mouseLastY >= 0 ) {
			d_x -= (x-d_mouseLastX) / d_zoom;
			d_y -= (y-d_mouseLastY) / d_zoom;
			d_mouseLastX = x;
			d_mouseLastY = y;
		}
	}
}


std::vector<ProcessFunction> FrameDisplayer::Prepare(size_t maxProcess, const cv::Size & size) {

	return {[this] ( const Frame::Ptr & , const cv::Mat & upstream, fort::hermes::FrameReadout & , cv::Mat & result) {
			size_t dataSize = upstream.dataend - upstream.datastart;
			if ( dataSize > d_data.size() ) {
				d_data.resize(dataSize);
			}


			cv::Mat out(upstream.rows, upstream.cols,upstream.type(),&(d_data[0]));
			upstream.copyTo(out);


			cv::imshow("artemis output",cv::Mat(out,BuildROI(upstream.size())));

			int8_t key = cv::waitKey(1);
			if (key != -1) {
				LOG(INFO) << "key press: " << key << " dec: " << (int) key;
			}
			if ( d_highlight < 0 ) {
				if ( key == 'z' ) {
					if ( d_zoom == MinZoom ) {
						d_zoom = MaxZoom;
					} else {
						d_zoom = MinZoom;
					}
				}

				if ( key == 'i' ) {
					d_ddProcess->ToggleDrawID();
				}

				if ( key == 'h' ) {
					if ( d_oWriter->HasMessage() ) {
						d_oWriter->SetMessage({});
					} else {
						d_oWriter->SetMessage({
						                       "                                  ",
						                       " Key bindings:                    ",
						                       " <up>: Zoom in                    ",
						                       " <down>: Zoom out                 ",
						                       " mouse: panning field of view     ",
						                       " z: Toggle min/max zoom           ",
						                       " t: Toggle highlight for a tag ID ",
						                       " i: Toggle ID drawing             ",
						                       " h: Toggle this help message      ",
						                       "                                  ",
							});
					}
				}


				if ( key == 82 ) {
					d_zoom = clamp(d_zoom + ZoomIncrement,MinZoom,MaxZoom);
				}

				if ( key == 84 ) {
					d_zoom = clamp(d_zoom - ZoomIncrement,MinZoom,MaxZoom);
				}

				//quit when we press quit
				if ( key == 'q' ||  key == 27) {
					if ( d_inhibQuit == false ) {
						std::raise(SIGINT);
					}
				}

				if ( d_ddProcess && key == 't' ) {
					d_highlight = 0;
					if ( d_oWriter ) {
						d_oWriter->SetPrompt("Tag to toggle highlight");
						d_oWriter->SetPromptValue("");
					}
				}
			} else {
				if ( key <= '9' && key >= '0' ) {
					d_highlight = d_highlight * 10 + (key - '0');
					if ( d_oWriter ) {
						std::ostringstream os;
						os << d_highlight;
						d_oWriter->SetPromptValue(os.str());
					}
				}

				if ( d_ddProcess && (key == 't' || key == 10) ) {
					d_ddProcess->ToggleHighlighted(d_highlight);
					d_highlight = -1;
					if ( d_oWriter ) {
						d_oWriter->SetPrompt("");
						d_oWriter->SetPromptValue("");
					}

				}
			}
		}
	};
}
