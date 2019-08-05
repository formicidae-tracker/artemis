#pragma once

#include  "ProcessDefinition.h"


#include <asio/io_service.hpp>

#include <string>
#include <mutex>
#include <set>

class AntCataloguerProcess : public ProcessDefinition {
public:
	AntCataloguerProcess(const std::string & savepath,
	                     size_t newAntROISize);
	virtual ~AntCataloguerProcess();

	virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);


private :
	void CheckForNewAnts( const Frame::Ptr & ptr,
	                      const fort::hermes::FrameReadout & readout,
	                      size_t start=0,
	                      size_t stride=1);

	cv::Rect GetROIForAnt(int x, int y, const cv::Size & frameSize);
	size_t BoundDimension(int xy, size_t max);

	std::mutex         d_mutex;
	std::string        d_savePath;
	std::set<int32_t>  d_known;
	const size_t       d_newAntROISize;
};
