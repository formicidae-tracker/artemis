#pragma once

#include <string>
#include <functional>
#include <memory>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <opencv2/core.hpp>

#include "DetectionConfig.h"

class InterprocessBuffer {
public:
	class Detection {
	public:
		double X,Y,Theta;
		uint16_t  ID;
	};

	InterprocessBuffer(const std::string & region);
	InterprocessBuffer(const std::string & region, size_t width, size_t height, const DetectionConfig & config);
	~InterprocessBuffer();

	const static size_t DetectionSize = 1024;

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
		uint64_t Size;
		DetectionConfig Config;
	};


	static size_t NeededSize(size_t width,size_t height);



	std::function<void ()> d_sharedMemoryRemover;
	std::unique_ptr<boost::interprocess::shared_memory_object> d_sharedMemory;
	std::unique_ptr<boost::interprocess::mapped_region>        d_mapping;

	Header    * d_header;
	Detection * d_detections;
	cv::Mat     d_image;
};
