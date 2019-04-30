#pragma once

#include <string>
#include <functional>
#include <memory>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <opencv2/core.hpp>

#include "DetectionConfig.h"

#define DETECTION_SIZE ((size_t)1024)


class InterprocessManager {
public:
	typedef std::shared_ptr<InterprocessManager> Ptr;
	InterprocessManager(bool isMain = false);
	~InterprocessManager();

	void WaitAllFinished();
	void WaitForNewJob();
	void PostNewJob();
	void PostJobFinished();


private:
	friend class InterprocessBuffer;
	class Shared {
	public:
		Shared();

		boost::interprocess::interprocess_mutex      d_mutex;
		boost::interprocess::interprocess_condition  d_newJob;
		bool                                         d_hasNewJob;
		int                                          d_jobs;
		boost::interprocess::interprocess_condition  d_ended;
	};


	std::function<void ()>                                     d_sharedMemoryCleaner;
	std::unique_ptr<boost::interprocess::shared_memory_object> d_sharedMemory;
	std::unique_ptr<boost::interprocess::mapped_region>        d_mapping;
	Shared *                                                   d_shared;
};

class InterprocessBuffer {
public:
	class Detection {
	public:
		double X,Y,Theta;
		uint16_t  ID;
	};

	InterprocessBuffer(const InterprocessManager::Ptr & manager, size_t ID);
	InterprocessBuffer(const InterprocessManager::Ptr & manager, size_t ID, const cv::Rect & roi, const DetectionConfig & config);
	~InterprocessBuffer();




	Detection * Detections();
	size_t &    DetectionsSize();
	cv::Mat &   Image();
	uint64_t &  TimestampIn();
	uint64_t &  TimestampOut();

	const DetectionConfig & Config() const;
private:

	class Header {
	public:
		uint64_t TimestampIn;
		uint64_t TimestampOut;
		uint64_t Width;
		uint64_t Height;
		uint64_t XOffset;
		uint64_t YOffset;
		uint64_t Size;
		DetectionConfig Config;
	};


	static size_t NeededSize(size_t width,size_t height);
	static std::string Name(size_t ID);

	InterprocessManager::Ptr  d_manager;

	std::function<void ()> d_sharedMemoryRemover;
	std::unique_ptr<boost::interprocess::shared_memory_object> d_sharedMemory;
	std::unique_ptr<boost::interprocess::mapped_region>        d_mapping;

	Header    * d_header;
	Detection * d_detections;
	cv::Mat     d_image;
};
