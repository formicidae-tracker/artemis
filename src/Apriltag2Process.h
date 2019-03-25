#pragma once

#include "ProcessDefinition.h"

#include <apriltag/apriltag.h>

class Apriltag2Process : public ProcessDefinition {
public:
	Apriltag2Process(const std::string & tagFamily);
	virtual ~Apriltag2Process();

	virtual std::vector<ProcessFunction> Prepare(size_t nbProcess, const cv::Size & size);
	virtual void Finalize(const cv::Mat & upstream, fort::FrameReadout & readout, cv::Mat & result);

private:
	typedef std::unique_ptr<apriltag_family_t, std::function <void (apriltag_family_t *) > > FamilyPtr;
	static FamilyPtr OpenFamily(const std::string & name);

	FamilyPtr d_family;
	std::unique_ptr<apriltag_detector_t,std::function<void (apriltag_detector_t*)> > d_detector;


	std::vector<zarray_t*> d_results;
	std::vector<size_t> d_offsets;
	std::set<int32_t> d_known;
};
