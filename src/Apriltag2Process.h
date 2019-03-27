#pragma once

#include "ProcessDefinition.h"

#include <apriltag/apriltag.h>


class AprilTag2Detector {
public:
	typedef std::shared_ptr<AprilTag2Detector> Ptr;

	struct Config {
		std::string Family;
	};

	class ROITagDetection : public ProcessDefinition {
	public:
		virtual ~ROITagDetection();

		virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);
	private :

		ROITagDetection(const AprilTag2Detector::Ptr & parent);

		ROITagDetection(const ROITagDetection & ) = delete;
		ROITagDetection & operator=(const ROITagDetection &) = delete;

		AprilTag2Detector::Ptr d_parent;
		friend class AprilTag2Detector;
	};

	class TagMerging : public ProcessDefinition {
	public:
		virtual ~TagMerging();

		virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);
	private:
		TagMerging(const AprilTag2Detector::Ptr & parent);

		TagMerging(const TagMerging & ) = delete;
		TagMerging & operator=(const TagMerging &) = delete;


		AprilTag2Detector::Ptr d_parent;
		friend class AprilTag2Detector;
	};

	static ProcessQueue Create(const Config & config);

	~AprilTag2Detector();


private:
	friend class ROITagDetection;
	friend class TagMerging;

	AprilTag2Detector(const Config & config);

	typedef std::unique_ptr<apriltag_family_t, std::function <void (apriltag_family_t *) > > FamilyPtr;
	static FamilyPtr OpenFamily(const std::string & name);
	FamilyPtr d_family;
	std::unique_ptr<apriltag_detector_t,std::function<void (apriltag_detector_t*)> > d_detector;

	std::vector<zarray_t*> d_results;
	std::vector<size_t> d_offsets;
	std::set<int32_t> d_known;
};
