#include "WatermarkingProcess.h"

#include <opencv2/imgproc.hpp>

WatermarkingProcess::WatermarkingProcess(const std::string & watermark)
	: d_text(watermark) {

	d_fontface = cv::FONT_HERSHEY_SIMPLEX;
	d_thickness = 5;
	d_fontscale = 3.0;
	d_textSize = cv::getTextSize(d_text, d_fontface, d_fontscale, d_thickness,
	                             &d_baseline);

}

WatermarkingProcess::~WatermarkingProcess() {
}

std::vector<ProcessFunction> WatermarkingProcess::Prepare(size_t maxProcess, const cv::Size & size) {


	return {[this](const Frame::Ptr & frame,
	                 const cv::Mat &,
	                 fort::hermes::FrameReadout & readout,
	                 cv::Mat & result) {
		        cv::Point org(result.cols/2 - d_textSize.width/2,result.rows/2- d_textSize.height /2);
		        if ( d_textSize.width > result.cols || d_textSize.height >= result.rows ) {
			        org.x = 0;
			        org.y = 0;
		        }
		        cv::putText(result,d_text,org,d_fontface,d_fontscale,cv::Scalar(0x00,0xff,0xff),d_thickness);
	        }
	};
}
