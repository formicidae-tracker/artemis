#include "AntCataloguerProcess.h"

#include <opencv2/highgui/highgui.hpp>

#include <glog/logging.h>

#include "utils/PosixCall.h"

#include <asio/posix/stream_descriptor.hpp>
#include <asio/write.hpp>

AntCataloguerProcess::AntCataloguerProcess(const std::string & savePath,
                                           size_t newAntROISize)
	: d_savePath(savePath)
	, d_newAntROISize(newAntROISize) {

	if ( d_savePath.empty() ) {
		throw std::invalid_argument("I need a path");
	}

}

AntCataloguerProcess::~AntCataloguerProcess() {};

std::vector<ProcessFunction> AntCataloguerProcess::Prepare(size_t maxProcess, const cv::Size &) {
	std::vector<ProcessFunction> res;
	res.reserve(maxProcess);

	for (size_t i = 0; i < maxProcess; ++i) {
		res.push_back([this,i,maxProcess](const Frame::Ptr & frame,
		                                  const cv::Mat & upstream,
		                                  fort::hermes::FrameReadout & readout,
		                                  cv::Mat & result) {
			              CheckForNewAnts(frame,readout,i,maxProcess);
		              });
	};
	return res;
}


void AntCataloguerProcess::CheckForNewAnts( const Frame::Ptr & frame,
                                            const fort::hermes::FrameReadout & readout,
                                            size_t start,
                                            size_t stride) {
	auto FID = frame->ID();
	for (size_t i = start; i < readout.ants_size(); i += stride) {
		auto a = readout.ants(i);
		int32_t ID = a.id();
		{
			std::lock_guard<std::mutex> lock(d_mutex);
			if ( d_known.count(ID) != 0 ) {
				continue;
			} else {
				d_known.insert(ID);
			}
		}
		cv::Rect roi(cv::Point2d(((size_t)a.x())-d_newAntROISize/2,
		                         ((size_t)a.y())-d_newAntROISize/2),
		             cv::Size(d_newAntROISize,
		                      d_newAntROISize));

		std::vector<uint8_t> pngData;
		cv::imencode(".png",cv::Mat(frame->ToCV(),roi),pngData);

		std::ostringstream oss;
		oss << d_savePath << "/ant_" << ID << "_frame_" << FID << ".png";
		int fd = open(oss.str().c_str(), O_CREAT|O_WRONLY,0644);
		if (fd == -1) {
			LOG(ERROR) << "Could not save ant " << ID << " in '" << oss.str() << "': " << ARTEMIS_SYSTEM_ERROR(open,errno).what();
			std::lock_guard<std::mutex> lock(d_mutex);
			d_known.erase(ID);
			return;
		}

		size_t writen = 0;
		while ( writen < pngData.size() ) {
			int res = write(fd,&(pngData[writen]),pngData.size() - writen);
			if ( res == -1 ) {
				LOG(ERROR) << "Could not save ant " << ID << " in '" << oss.str() << "': " << ARTEMIS_SYSTEM_ERROR(write,errno).what();
				break;
			}
			writen += res;
		}
		close(fd);
		if ( writen < pngData.size() ) {
			std::lock_guard<std::mutex> lock(d_mutex);
			d_known.erase(ID);
		}
		return;
	}
}
