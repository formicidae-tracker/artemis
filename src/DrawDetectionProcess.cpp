#include "DrawDetectionProcess.h"

#include <opencv2/imgproc/imgproc.hpp>

#include <Eigen/Geometry>

#include <glog/logging.h>

#include <iomanip>

#define GHEIGHT 5
#define GWIDTH 3

class HexadecimalDrawer {
public:
	HexadecimalDrawer()
		: d_glyphs(256,cv::Mat(GHEIGHT+2,GWIDTH+2,CV_8UC3,cv::Vec3b(0,0,0))) {

		static std::vector<std::pair<char,std::vector<uint8_t> >> rawGlyphs
			= {
			   { '0',
			     { 1, 1, 1,
			       1, 0, 1,
			       1, 0, 1,
			       1, 0, 1,
			       1, 1, 1 }
			   },
			   { '1',
			     { 0, 1, 0,
			       0, 1, 0,
			       0, 1, 0,
			       0, 1, 0,
			       0, 1, 0
			     }
			   },
			   { '2',
			     { 1, 1, 1,
			       0, 0, 1,
			       1, 1, 1,
			       1, 0, 0,
			       1, 1, 1 }
			   },
			   { '3',
			     { 1, 1, 1,
			       0, 0, 1,
			       0, 1, 1,
			       0, 0, 1,
			       1, 1, 1 }
			   },
			   { '4',
			     { 1, 0, 1,
			       1, 0, 1,
			       1, 1, 1,
			       0, 0, 1,
			       0, 0, 1 }
			   },
			   { '5',
			     { 1, 1, 1,
			       1, 0, 0,
			       1, 1, 1,
			       0, 0, 1,
			       1, 1, 1 }
			   },
			   { '6',
			     { 1, 1, 1,
			       1, 0, 0,
			       1, 1, 1,
			       1, 0, 1,
			       1, 1, 1 }
			   },
			   { '7',
			     { 1, 1, 1,
			       0, 0, 1,
			       0, 0, 1,
			       0, 0, 1,
			       0, 0, 1 }
			   },
			   { '8',
			     { 1, 1, 1,
			       1, 0, 1,
			       1, 1, 1,
			       1, 0, 1,
			       1, 1, 1 }
			   },
			   { '9',
			     { 1, 1, 1,
			       1, 0, 1,
			       1, 1, 1,
			       0, 0, 1,
			       0, 0, 1}
			   },
			   { 'x',
			     { 0, 0, 0,
			       0, 0, 0,
			       1, 0, 1,
			       0, 1, 0,
			       1, 0, 1}
			   },
			   { 'a',
			     { 0, 1, 0,
			       1, 0, 1,
			       1, 1, 1,
			       1, 0, 1,
			       1, 0, 1}
			   },
			   { 'b',
			     { 1, 1, 0,
			       1, 0, 1,
			       1, 1, 0,
			       1, 0, 1,
			       1, 1, 0}
			   },
			   { 'c',
			     { 0, 1, 1,
			       1, 0, 0,
			       1, 0, 0,
			       1, 0, 0,
			       0, 1, 1}
			   },
			   { 'd',
			     { 1, 1, 0,
			       1, 0, 1,
			       1, 0, 1,
			       1, 0, 1,
			       1, 1, 0}
			   },
			   { 'e',
			     { 1, 1, 1,
			       1, 0, 0,
			       1, 1, 0,
			       1, 0, 0,
			       1, 1, 1}
			   },
			   { 'f',
			     { 1, 1, 1,
			       1, 0, 0,
			       1, 1, 0,
			       1, 0, 0,
			       1, 0, 0}
			   },
		};

		for ( const auto & iter : rawGlyphs ) {
			//copy a new mat otherwise we manipulate the same copy
			cv::Mat g(GHEIGHT+2,GWIDTH+2,CV_8UC3,cv::Vec3b(0,0,0));
			d_glyphs[size_t(iter.first)] = g;
			size_t i = 0;
			for (size_t iy = 1; iy <= GHEIGHT; ++iy) {
				for (size_t ix = 1; ix <= GWIDTH; ++ix ) {
					if ( iter.second[i] == 1 ) {
						g.at<cv::Vec3b>(iy,ix) = cv::Vec3b(0xff,0xff,0xff);
					}
					++i;
				}
			}
		}
	}

	void draw(cv::Mat & dest,cv::Point topLeft,
	          uint32_t number,const cv::Vec3b & color) const {

		std::ostringstream os;
		os << "0x" << std::hex << std::setw(3) << std::setfill('0') << number;
		for ( const auto & c : os.str() ) {
			const auto & g = d_glyphs[size_t(c)];
			g.copyTo(dest(cv::Rect(topLeft,cv::Size(GWIDTH+2,GHEIGHT+2))));
			topLeft += cv::Point(5,0);
		}
	}

private:
	std::vector<cv::Mat>  d_glyphs;

};

DrawDetectionProcess::DrawDetectionProcess()
	: d_drawIDs(false) {
}

DrawDetectionProcess::~DrawDetectionProcess() {
}


void DrawDetectionProcess::AddHighlighted(uint32_t highlighted ) {
	LOG(INFO) << "Highlighting " << highlighted;
	d_highlighted.insert(highlighted);
}

void DrawDetectionProcess::RemoveHighlighted(uint32_t highlighted ) {
	LOG(INFO) << "Unhighlighting " << highlighted;
	d_highlighted.erase(highlighted);
}

void DrawDetectionProcess::ToggleHighlighted(uint32_t highlighted ) {
	if ( IsHighlighted(highlighted)  == false ) {
		AddHighlighted(highlighted);
	} else {
		RemoveHighlighted(highlighted);
	}
}


std::vector<ProcessFunction> DrawDetectionProcess::Prepare(size_t maxProcess, const cv::Size &) {
	std::vector<ProcessFunction> res;
	for( size_t i = 0 ; i< maxProcess; ++i) {
		res.push_back([this,i,maxProcess](const Frame::Ptr & frame,
		                             const cv::Mat &,
		                             fort::hermes::FrameReadout & readout,
		                             cv::Mat & result) {
			              double ratio = double(result.rows) / double(frame->ToCV().rows);
			              DrawAnts(i,maxProcess,readout,result,ratio);

		              });
	}
	return res;
}



void DrawDetectionProcess::DrawAnts(size_t start, size_t stride, const fort::hermes::FrameReadout & readout,cv::Mat & result, double ratio) {
	for (size_t i = start; i < readout.tags_size(); i += stride ) {
		DrawAnt(readout.tags(i),result,ratio);
	}
}


void DrawDetectionProcess::ToggleDrawID() {
	d_drawIDs = !d_drawIDs;
}


void DrawDetectionProcess::DrawAnt(const fort::hermes::Tag & a, cv::Mat & result,double ratio) {
	cv::Vec3b foreground(0xff,0xff,0xff);
	static cv::Vec3b background(0x00,0x00,0x00);
	static cv::Scalar circleColor = cv::Scalar(0x00,0x00,0xff);
	static cv::Scalar circleHighlight = cv::Scalar(0xff,0xff,0x00);

	Eigen::Vector2d center(a.x(),a.y());
	center *= ratio;
	if ( IsHighlighted(a.id()) == true ) {
		foreground = cv::Vec3b(0xff,0xff,0x00);
		cv::circle(result,cv::Point(center.x(),center.y()),6,circleHighlight,-1);
	} else {
		cv::circle(result,cv::Point(center.x(),center.y()),3,circleColor,-1);
	}

	if ( d_drawIDs == false ) {
		return;
	}

	static HexadecimalDrawer hdrawer;

	hdrawer.draw(result,
	             cv::Point(center.x() + 6, center.y()-2),
	             a.id(),foreground);

}
