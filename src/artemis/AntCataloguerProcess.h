#pragma once

#include  "ProcessDefinition.h"


#include <asio/io_service.hpp>

#include <string>
#include <mutex>
#include <set>

class AntCataloguerProcess {
public:
	AntCataloguerProcess(asio::io_service & service,
	                     const std::string & savepath,
	                     size_t newAntROISize);
	virtual ~AntCataloguerProcess();


	void operator() (const Frame::Ptr & frame,
	                 const cv::Mat & upstream,
	                 const fort::FrameReadout & readout,
	                 cv::Mat & result);


private :
	void CheckForNewAnts( const Frame::Ptr & frame,
	                      const fort::FrameReadout & readout,
	                      size_t start=0,
	                      size_t stride=1);

	asio::io_service & d_service;
	std::string        d_savePath;
	std::mutex         d_mutex;
	std::set<int32_t>  d_known;
	const size_t       d_newAntROISize;
};
