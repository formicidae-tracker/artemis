#include "InterprocessBuffer.h"


using namespace boost::interprocess;

size_t InterprocessBuffer::NeededSize(size_t width,size_t height) {
	return width * height * sizeof(uint8_t) + DetectionSize * sizeof(Detection) + sizeof(Header);
}

InterprocessBuffer::InterprocessBuffer(const std::string & region,
                                       size_t width, size_t height,
                                       const DetectionConfig & config) {
	shared_memory_object::remove(region.c_str());
	d_sharedMemoryRemover = [region]() { shared_memory_object::remove(region.c_str()); };

	d_sharedMemory = std::unique_ptr<shared_memory_object>(new shared_memory_object(create_only,region.c_str(),read_write));

	d_sharedMemory->truncate(NeededSize(width,height));
	d_mapping = std::unique_ptr<mapped_region> ( new mapped_region(*d_sharedMemory,read_write));

	d_header               = reinterpret_cast<Header*>(d_mapping->get_address());
	d_header->TimestampIn  = -1;
	d_header->TimestampOut = -1;
	d_header->Width        = width;
	d_header->Height       = height;
	d_header->Size         = 0;
	d_header->Config       = config;

	d_detections = (Detection*)(d_header + 1);
	d_image = cv::Mat(height,width,CV_8U,d_detections + DetectionSize);
}

InterprocessBuffer::~InterprocessBuffer() {
	d_sharedMemoryRemover();
}

InterprocessBuffer::InterprocessBuffer(const std::string & region ) {
	d_sharedMemoryRemover = [](){};

	d_sharedMemory = std::unique_ptr<shared_memory_object>(new shared_memory_object(open_only,region.c_str(),read_write));
	d_mapping = std::unique_ptr<mapped_region> ( new mapped_region(*d_sharedMemory,read_write));
	if (d_mapping->get_size() < sizeof(Header) ) {
		throw std::runtime_error("Invalid Shared Memory size");
	}
	d_header = reinterpret_cast<Header*>(d_mapping->get_address());
	if ( d_mapping->get_size() < NeededSize(d_header->Width,d_header->Height) ) {
		throw std::runtime_error("Insufficient Shared Memory size");
	}
	d_detections = (Detection*)(d_header + 1);
	d_image = cv::Mat(d_header->Height,d_header->Width,CV_8U,d_detections + DetectionSize);
}

InterprocessBuffer::Detection * InterprocessBuffer::Detections() {
	return d_detections;
}

size_t & InterprocessBuffer::DetectionsSize() {
	return d_header->Size;
}

cv::Mat & InterprocessBuffer::Image() {
	return d_image;
}

uint64_t & InterprocessBuffer::TimestampIn() {
	return d_header->TimestampIn;
}

uint64_t &  InterprocessBuffer::TimestampOut() {
	return d_header->TimestampOut;
}

const DetectionConfig & InterprocessBuffer::Config() const {
	return d_header->Config;
}
