#pragma once

#include  "ProcessDefinition.h"


#include <string>
#include <mutex>
#include <set>
#include <chrono>

class LegacyAntCataloguerProcess : public ProcessDefinition {
public:
	LegacyAntCataloguerProcess(const std::string & savepath,
	                           std::chrono::seconds antRenewPeriod);
	virtual ~LegacyAntCataloguerProcess();

	virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);

private :
	typedef std::chrono::time_point<std::chrono::system_clock> TimePoint;
	typedef std::set<int32_t> SetOfAnts;

	void CheckForNewAnts( const Frame::Ptr & ptr,
	                      const fort::hermes::FrameReadout & readout,
	                      const TimePoint & now,
	                      size_t start=0,
	                      size_t stride=1);

	std::string          d_savePath;
	SetOfAnts            d_known;
	TimePoint            d_beginPeriod;
	std::chrono::seconds d_antRenewPeriod;
};
