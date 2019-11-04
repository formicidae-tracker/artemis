#include "OverlayWriter.h"

#include "utils/PosixCall.h"

#include <iomanip>

#include "artemis-config.h"

#include <google/protobuf/util/time_util.h>

OverlayWriter::OverlayWriter(bool drawStatistic)
	: d_drawStatistics(drawStatistic) {
	LoadFontData();
}

OverlayWriter::~OverlayWriter() {
}

void OverlayWriter::SetPrompt(const std::string & prompt) {
	d_prompt = prompt;
}
void OverlayWriter::SetPromptValue(const std::string & promptValue) {
	d_value = promptValue;
}



std::vector<ProcessFunction> OverlayWriter::Prepare(size_t maxProcess, const cv::Size &) {
	return {[this](const Frame::Ptr & frame,
	                            const cv::Mat &,
	                            fort::hermes::FrameReadout & readout,
	                            cv::Mat & result) {
		        DrawFrameNumber(readout, result);
		        DrawDate(readout, result);
		        size_t promptLine = 1;
		        if ( d_drawStatistics == true ) {
			        promptLine = 3;
			        DrawStatistics(readout,result);
		        }
		        DrawPrompt(d_prompt,d_value,promptLine,result);
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
	for ( size_t iy = 0;  iy < GLYPH_HEIGHT; ++iy ) {
		size_t yy = y+iy;
		size_t ic = 0;
		for ( auto c : text ) {
			uint8_t xdata = fontdata[c][iy];
			for ( size_t ix = 0; ix < GLYPH_WIDTH; ++ix) {
				size_t xx = x + TOTAL_GLYPH_WIDTH * ic + ix;
				if ((xdata & (1 << (7-ix))) != 0) {
					img.at<cv::Vec3b>(yy,xx) = fg;
				} else {
					img.at<cv::Vec3b>(yy,xx) = bg;
				}
			}
			size_t xx = x + TOTAL_GLYPH_WIDTH * ic + GLYPH_WIDTH;
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

void OverlayWriter::DrawDate(const fort::hermes::FrameReadout & readout,cv::Mat & result) {
	std::ostringstream os;
	auto time = google::protobuf::util::TimeUtil::TimestampToTimeT(readout.time());
	os << std::put_time(std::localtime(&time),"%c %Z");
	size_t length = os.str().size();
	DrawText(result, os.str(), result.cols-TOTAL_GLYPH_WIDTH*length, 0);
}

void OverlayWriter::DrawStatistics(const fort::hermes::FrameReadout & readout,cv::Mat & result) {
	std::ostringstream os;
	os << "Detected Tags: " << readout.tags_size();
	DrawText(result, os.str(), 0, GLYPH_HEIGHT);
	os.str("");
	os.clear();
	os << "Detected Quads: " << readout.quads();
	DrawText(result, os.str(), 0, 2*GLYPH_HEIGHT);
}

void OverlayWriter::DrawPrompt(const std::string & prompt, const std::string & value, size_t line,cv::Mat & result) {
	if ( prompt.empty() ) {
		return;
	}
	std::ostringstream os;
	os << prompt << ": " << value;
	DrawText(result, os.str(), 0, line *GLYPH_HEIGHT);
}
