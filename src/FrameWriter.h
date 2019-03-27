#pragma once

#include <opencv2/core.hpp>

class FrameWriter {
public :
	FrameWriter();
	~FrameWriter();

	void WriteFrameAsync(uint64_t timestamp, uint64_t frameID, const cv::Mat & mat);
	bool IsDone();

private:
	uint8_t d_header[2*sizeof(uint64_t)+2*sizeof(size_t)];
	cv::Mat d_data;

	bool   d_done;
	size_t d_headerWriten;
	size_t d_dataWriten;
};
