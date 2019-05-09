#pragma once

#include  "ProcessDefinition.h"


#include <asio/io_service.hpp>

#include <string>
#include <mutex>
#include <set>

class AntCataloguerProcess : public ProcessDefinition {
public:
	AntCataloguerProcess(asio::io_service & service,
	                     const std::string & savepath,
	                     size_t newAntROISize);
	virtual ~AntCataloguerProcess();

	virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);


private :
	void CheckForNewAnts( const Frame::Ptr & ptr,
	                      const fort::hermes::FrameReadout & readout,
	                      size_t start=0,
	                      size_t stride=1);

	asio::io_service & d_service;
	std::mutex         d_mutex;
	std::string        d_savePath;
	std::set<int32_t>  d_known;
	const size_t       d_newAntROISize;
};
