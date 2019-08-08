#pragma once

#include  "ProcessDefinition.h"


#include <string>
#include <mutex>
#include <map>
#include <chrono>

class AntCataloguerProcess : public ProcessDefinition {
public:
	AntCataloguerProcess(const std::string & savepath,
	                     size_t newAntROISize,
	                     std::chrono::seconds antRenewPeriod);
	virtual ~AntCataloguerProcess();

	virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);

private :
	typedef std::chrono::time_point<std::chrono::system_clock> TimePoint;
	typedef std::map<int32_t, TimePoint > LastSeenByID;

	void CheckForNewAnts( const Frame::Ptr & ptr,
	                      const fort::hermes::FrameReadout & readout,
	                      const TimePoint & now,
	                      size_t start=0,
	                      size_t stride=1);

	cv::Rect GetROIForAnt(int x, int y, const cv::Size & frameSize);
	size_t BoundDimension(int xy, size_t max);

	std::mutex         d_mutex;
	std::string        d_savePath;
	LastSeenByID       d_known;
	const size_t       d_newAntROISize;
	std::chrono::seconds d_antRenewPeriod;
};
