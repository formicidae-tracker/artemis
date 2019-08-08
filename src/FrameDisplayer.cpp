#include "FrameDisplayer.h"

#include <opencv2/highgui/highgui.hpp>

#include <glog/logging.h>

FrameDisplayer::FrameDisplayer() {
	cv::namedWindow("artemis output",cv::WINDOW_NORMAL | CV_WINDOW_KEEPRATIO | CV_GUI_EXPANDED);
}

FrameDisplayer::~FrameDisplayer() {

}

std::vector<ProcessFunction> FrameDisplayer::Prepare(size_t maxProcess, const cv::Size &) {
	return {[this] ( const Frame::Ptr & , const cv::Mat & upstream, fort::hermes::FrameReadout & , cv::Mat & result) {
			size_t dataSize = upstream.dataend - upstream.datastart;
			if ( dataSize > d_data.size() ) {
				d_data.resize(dataSize);
			}

			cv::Mat out(upstream.rows, upstream.cols,upstream.type(),&(d_data[0]));
			upstream.copyTo(out);


			cv::imshow("artemis output",out);//cv::Mat(out,cv::Rect(cv::Point(0,0),cv::Size(out.cols/2,out.rows/2))));
			cv::waitKey(1);
		}
	};
}
