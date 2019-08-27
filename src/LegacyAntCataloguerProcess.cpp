#include "LegacyAntCataloguerProcess.h"

#include <opencv2/highgui/highgui.hpp>

#include <glog/logging.h>

#include "utils/PosixCall.h"

#include <asio/posix/stream_descriptor.hpp>
#include <asio/write.hpp>

LegacyAntCataloguerProcess::LegacyAntCataloguerProcess(const std::string & savePath,
                                                       std::chrono::seconds antRenewPeriod)
	: d_savePath(savePath)
	, d_antRenewPeriod(antRenewPeriod)
	, d_beginPeriod (std::chrono::system_clock::now()){
	if ( d_savePath.empty() ) {
		throw std::invalid_argument("I need a path");
	}
}

LegacyAntCataloguerProcess::~LegacyAntCataloguerProcess() {};

std::vector<ProcessFunction> LegacyAntCataloguerProcess::Prepare(size_t maxProcess, const cv::Size & size) {
	std::vector<ProcessFunction> res;
	res.reserve(1);
	TimePoint now = std::chrono::system_clock::now();

	if ((now - d_beginPeriod) >= d_antRenewPeriod) {
		d_beginPeriod = now;
		d_known.clear();
	}

	res.push_back([this](const Frame::Ptr & frame,
	                     const cv::Mat & upstream,
	                     fort::hermes::FrameReadout & readout,
	                     cv::Mat & result) {
		              bool hasNew = false;
		              auto FID = frame->ID();
		              for (size_t i = 0; i < readout.ants_size(); i += 1) {
			              auto a = readout.ants(i);
			              int32_t ID = a.id();
			              if (d_known.count(ID) == 0 ) {
				              hasNew = true;
				              break;
			              }
		              }
		              if ( hasNew == false ) {
			              return;
		              }

		              std::vector<uint8_t> pngData;
		              cv::imencode(".png",frame->ToCV(),pngData);

		              std::ostringstream oss;
		              oss << d_savePath << "/frame_" << FID << ".png";
		              int fd = open(oss.str().c_str(), O_CREAT|O_WRONLY,0644);
		              if (fd == -1) {
			              LOG(ERROR) << "Could not save frame " << FID << " in '" << oss.str() << "': " << ARTEMIS_SYSTEM_ERROR(open,errno).what();
			              return;
		              }

		              size_t writen = 0;
		              while ( writen < pngData.size() ) {
			              int res = write(fd,&(pngData[writen]),pngData.size() - writen);
			              if ( res == -1 ) {
				              LOG(ERROR) << "Could not save frame " << FID << " in '" << oss.str() << "': " << ARTEMIS_SYSTEM_ERROR(write,errno).what();
				              break;
			              }
			              writen += res;
		              }
		              close(fd);
		              if ( writen < pngData.size() ) {
			              return;
		              }

		              for (size_t i = 0; i < readout.ants_size(); i += 1) {
			              auto a = readout.ants(i);
			              int32_t ID = a.id();
			              d_known.insert(ID);
		              }

	              });
	return res;
}
