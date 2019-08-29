#include "OverlayWriter.h"

#include "utils/PosixCall.h"

#include <iomanip>

#include "artemis-config.h"

OverlayWriter::OverlayWriter(){
	LoadFontData();
}

OverlayWriter::~OverlayWriter() {
}


std::vector<ProcessFunction> OverlayWriter::Prepare(size_t maxProcess, const cv::Size &) {
	return {[this](const Frame::Ptr & frame,
	                            const cv::Mat &,
	                            fort::hermes::FrameReadout & readout,
	                            cv::Mat & result) {
		        DrawFrameNumber(readout, result);
	        }};
}

OverlayWriter::fontchar OverlayWriter::fontdata[256];


void OverlayWriter::LoadFontData() {
	static std::string fontpath = ARTEMIS_INSTALL_PREFIX "/share/artemis/vga.fon";
	FILE* f = fopen(fontpath.c_str(),"rb");
	if (!f) {
		throw std::system_error(errno, ARTEMIS_SYSTEM_CATEGORY(),"fopen('"+fontpath+"'):");
	}
	fread(fontdata, 4096, 1, f);
	fclose(f);
}

void OverlayWriter::DrawText(cv::Mat & img, const std::string & text, size_t x, size_t y) {
	static cv::Vec3b fg(255,255,255);
	static cv::Vec3b bg(0,0,0);
	for ( size_t iy = 0;  iy < 16; ++iy ) {
		size_t yy = y+iy;
		size_t ic = 0;
		for ( auto c : text ) {
			uint8_t xdata = fontdata[c][iy];
			for ( size_t ix = 0; ix < 8; ++ix) {
				size_t xx = x + 9 * ic + ix;
				if ((xdata & (1 << (7-ix))) != 0) {
					img.at<cv::Vec3b>(yy,xx) = fg;
				} else {
					img.at<cv::Vec3b>(yy,xx) = bg;
				}
			}
			size_t xx = x + 9 * ic + 8;
			if ( c >= 0xC0 && c <= 0xDF ) {
				if ( xdata & 1 ) {
					img.at<cv::Vec3b>(yy,xx) = fg;
				} else {
					img.at<cv::Vec3b>(yy,xx) = bg;
				}
			} else {
				img.at<cv::Vec3b>(yy,xx) = bg;
			}
			ic += 1;
		}
	}
}


void OverlayWriter::DrawFrameNumber(const fort::hermes::FrameReadout & readout,cv::Mat & result) {
	std::ostringstream os;
	os << "Frame "  << std::setw(8) << std::setfill('0') << readout.frameid();
	DrawText(result,os.str(),0,0);
}
