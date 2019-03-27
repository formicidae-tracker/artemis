#include "FrameWriter.h"

#include "utils/PosixCall.h"


#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


FrameWriter::FrameWriter()
	: d_done(true) {
	int flags = fcntl(STDOUT_FILENO,F_GETFL);
	if ( flags == -1 ) {
		throw ARTEMIS_SYSTEM_ERROR(fcntl,errno);
	}
	p_call(fcntl,STDOUT_FILENO,F_SETFL, flags | O_NONBLOCK);

}


FrameWriter::~FrameWriter() {}

void FrameWriter::WriteFrameAsync(uint64_t timestamp, uint64_t frameID, const cv::Mat & mat) {
	if (!IsDone() ) {
		throw std::runtime_error("busy");
	}
	d_headerWriten = 0;
	d_dataWriten = 0;
	memcpy(d_header,&timestamp,sizeof(uint64_t));
	memcpy(d_header+sizeof(uint64_t),&frameID,sizeof(uint64_t));
	size_t width = mat.cols;
	size_t height = mat.rows;

	memcpy(d_header+2*sizeof(uint64_t),&width,sizeof(size_t));
	memcpy(d_header+2*sizeof(uint64_t)+sizeof(size_t),&height,sizeof(size_t));

	d_data = mat;
	d_done = false;
	IsDone();
}

bool FrameWriter::IsDone() {
	if ( d_done == true ) {
		return d_done;
	}
	size_t dataSize = d_data.dataend - d_data.datastart;
	if ( d_dataWriten >= dataSize ) {
		d_done = true;
		d_data = cv::Mat();
		return true;
	}

	if (d_headerWriten < sizeof(d_header) ) {

		int writen = write(STDOUT_FILENO,&d_header[d_headerWriten],sizeof(d_header) - d_headerWriten);
		if (writen == -1 ) {
			std::error_code e(errno,ARTEMIS_SYSTEM_CATEGORY());
			if ( e == std::errc::resource_unavailable_try_again || e == std::errc::operation_would_block ) {
				return false;
			}
			throw ARTEMIS_SYSTEM_ERROR(write,errno);
		}
		d_headerWriten += writen;
		return false;
	}

	if ( d_dataWriten < dataSize ) {
		int writen = write(STDOUT_FILENO,&d_data.datastart + d_dataWriten, dataSize - d_dataWriten);
		if (writen == -1 ) {
			std::error_code e(errno,ARTEMIS_SYSTEM_CATEGORY());
			if ( e == std::errc::resource_unavailable_try_again || e == std::errc::operation_would_block ) {
				return false;
			}
			throw ARTEMIS_SYSTEM_ERROR(write,errno);
		}
		d_dataWriten += writen;
		return false;
	}
	return false;
}
