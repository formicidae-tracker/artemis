#include "AntCataloguerProcess.h"

#include <opencv2/highgui/highgui.hpp>

#include <glog/logging.h>

#include "utils/PosixCall.h"

#include <asio/posix/stream_descriptor.hpp>
#include <asio/write.hpp>

AntCataloguerProcess::AntCataloguerProcess(const std::string & savePath,
                                           size_t newAntROISize,
                                           std::chrono::seconds antRenewPeriod)
	: d_savePath(savePath)
	, d_newAntROISize(newAntROISize)
	, d_antRenewPeriod(antRenewPeriod) {

	if ( d_savePath.empty() ) {
		throw std::invalid_argument("I need a path");
	}

}

AntCataloguerProcess::~AntCataloguerProcess() {};

std::vector<ProcessFunction> AntCataloguerProcess::Prepare(size_t maxProcess, const cv::Size & size) {
	std::vector<ProcessFunction> res;
	res.reserve(maxProcess);
	if (size.width < d_newAntROISize || size.height < d_newAntROISize) {
		std::ostringstream os;
		os << "New Ant ROI Size (" << d_newAntROISize
		   << ")  is too large for the camera resolution ("
		   << size.width << "x" << size.height << ")";
		throw std::runtime_error(os.str());
	}
	TimePoint now = std::chrono::system_clock::now();

	for (size_t i = 0; i < maxProcess; ++i) {
		res.push_back([this,i,maxProcess,now](const Frame::Ptr & frame,
		                                      const cv::Mat & upstream,
		                                      fort::hermes::FrameReadout & readout,
		                                      cv::Mat & result) {
			              CheckForNewAnts(frame,readout,now,i,maxProcess);
		              });
	};
	return res;
}


size_t AntCataloguerProcess::BoundDimension(int xy, size_t max) {
	if (xy + d_newAntROISize > max ) {
		xy = max - d_newAntROISize;
	}
	if (xy < 0 ) {
		xy = 0;
	}
	return xy;
}


cv::Rect AntCataloguerProcess::GetROIForAnt(int x, int y, const cv::Size & frameSize) {
	return cv::Rect(cv::Point2d(BoundDimension(x - d_newAntROISize/2,frameSize.width),
	                            BoundDimension(y - d_newAntROISize/2,frameSize.height)),
	                cv::Size(d_newAntROISize,
	                         d_newAntROISize));
}


void AntCataloguerProcess::CheckForNewAnts( const Frame::Ptr & frame,
                                            const fort::hermes::FrameReadout & readout,
                                            const TimePoint & now,
                                            size_t start,
                                            size_t stride) {
	auto FID = frame->ID();
	for (size_t i = start; i < readout.tags_size(); i += stride) {
		auto a = readout.tags(i);
		int32_t ID = a.id();
		{
			std::lock_guard<std::mutex> lock(d_mutex);
			LastSeenByID::const_iterator fi = d_known.find(ID);

			if ( fi != d_known.end() && (now - fi->second) < d_antRenewPeriod ) {
				continue;
			} else {
				d_known[ID] = now ;
			}
		}


		std::vector<uint8_t> pngData;
		cv::imencode(".png",cv::Mat(frame->ToCV(),GetROIForAnt(a.x(),a.y(),frame->ToCV().size())),pngData);

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
