#include "InterprocessBuffer.h"

#include <mutex>

#include <glog/logging.h>

#define ARTEMIS_SHM_PREFIX "artemis"
#define ARTEMIS_SHM_MANAGER "artemis.Manager"

using namespace boost::interprocess;

InterprocessManager::Shared::Shared()
	: d_timestamp(std::numeric_limits<uint64_t>::max())
	, d_jobs(-1) {
}

InterprocessManager::InterprocessManager(bool isMain) {
	if (isMain == true) {
		shared_memory_object::remove(ARTEMIS_SHM_MANAGER);
		d_sharedMemoryCleaner = []() { shared_memory_object::remove(ARTEMIS_SHM_MANAGER); };

		d_sharedMemory = std::unique_ptr<shared_memory_object>(new shared_memory_object(create_only,ARTEMIS_SHM_MANAGER,read_write));

		d_sharedMemory->truncate(sizeof(Shared));

	} else {
		d_sharedMemoryCleaner = [](){};
		d_sharedMemory = std::unique_ptr<shared_memory_object>(new shared_memory_object(open_only,ARTEMIS_SHM_MANAGER,read_write));
	}

	d_mapping = std::unique_ptr<mapped_region>( new mapped_region(*d_sharedMemory,read_write) );

	if (isMain == true ) {
		d_shared = new (d_mapping->get_address()) Shared();
	} else {
		d_shared = reinterpret_cast<Shared*>(d_mapping->get_address());
	}
}

void InterprocessManager::WaitAllFinished() {
	std::unique_lock<interprocess_mutex> lock(d_shared->d_mutex);

	d_shared->d_ended.wait(lock,[this]()->bool{
			if (d_shared->d_jobs != 0 ) {
				return false;
			}
			d_shared->d_jobs = -1;
			return true;
		});
}

uint64_t InterprocessManager::WaitForNewJob(uint64_t oldTimestamp) {
	std::unique_lock<interprocess_mutex> lock(d_shared->d_mutex);
	uint64_t res;
	d_shared->d_newJob.wait(lock,[this,oldTimestamp,&res]()->bool{
			if (d_shared->d_timestamp == oldTimestamp) {
				return false;
			}
			if (d_shared->d_jobs < 0 ) {
				d_shared->d_jobs = 0;
			}
			d_shared->d_jobs += 1;
			res = d_shared->d_timestamp;
			return true;
		});
	return res;
}

void InterprocessManager::PostNewJob(uint64_t newJob) {
	std::lock_guard<interprocess_mutex> lock(d_shared->d_mutex);
	d_shared->d_timestamp = newJob;
	d_shared->d_newJob.notify_all();
}

void InterprocessManager::PostJobFinished() {
	std::lock_guard<interprocess_mutex> lock(d_shared->d_mutex);
	if ( d_shared->d_jobs > 0 ) {
		d_shared->d_jobs -= 1;
	}
	if ( d_shared->d_jobs == 0 ) {
		d_shared->d_ended.notify_all();
	}
}

InterprocessManager::~InterprocessManager() {
	d_sharedMemoryCleaner();
}

size_t InterprocessBuffer::NeededSize(size_t width,size_t height) {
	return width * height * sizeof(uint8_t) + DETECTION_SIZE * sizeof(Detection) + sizeof(Header);
}

std::string InterprocessBuffer::Name(size_t ID) {
	std::ostringstream os;
	os << ARTEMIS_SHM_PREFIX << "." << ID;
	return os.str();
}

InterprocessBuffer::InterprocessBuffer(const InterprocessManager::Ptr & manager,
                                       size_t ID,
                                       const cv::Rect & roi,
                                       const DetectionConfig & config)
	: d_manager(manager) {
	std::string region = Name(ID);
	shared_memory_object::remove(region.c_str());
	d_sharedMemoryRemover = [region]() { shared_memory_object::remove(region.c_str()); };

	d_sharedMemory = std::unique_ptr<shared_memory_object>(new shared_memory_object(create_only,region.c_str(),read_write));

	d_sharedMemory->truncate(NeededSize(roi.width,roi.height));
	d_mapping = std::unique_ptr<mapped_region> ( new mapped_region(*d_sharedMemory,read_write));

	LOG(INFO) << "Created '" << region << "' of size " << d_mapping->get_size() << " needed " << NeededSize(roi.width,roi.height);

	d_header               = reinterpret_cast<Header*>(d_mapping->get_address());
	d_header->TimestampIn  = std::numeric_limits<uint64_t>::max();
	d_header->TimestampOut = std::numeric_limits<uint64_t>::max();
	d_header->Width        = roi.width;
	d_header->Height       = roi.height;
	d_header->XOffset      = roi.x;
	d_header->YOffset      = roi.y;
	d_header->Size         = 0;
	d_header->Config       = config;

	d_detections = (Detection*)(d_header + 1);
	d_image = cv::Mat(roi.height,roi.width,CV_8U,d_detections + DETECTION_SIZE);
	LOG(INFO) << "Header: " << d_header << " size: " << (size_t)((uint8_t*)d_detections-(uint8_t*)d_header);
	LOG(INFO) << "Detection: " << d_detections << " size: " << (size_t)(d_image.datastart - (uint8_t*)d_detections);
	LOG(INFO) << "Image: " << d_image.datastart << " size: " << (size_t)(d_image.dataend - d_image.datastart);
}

InterprocessBuffer::~InterprocessBuffer() {
	d_sharedMemoryRemover();
}

InterprocessBuffer::InterprocessBuffer(const InterprocessManager::Ptr & manager,
                                       size_t ID )
	: d_manager(manager) {
	d_sharedMemoryRemover = [](){};
	std::string region = Name(ID);
	d_sharedMemory = std::unique_ptr<shared_memory_object>(new shared_memory_object(open_only,region.c_str(),read_write));
	d_mapping = std::unique_ptr<mapped_region> ( new mapped_region(*d_sharedMemory,read_write));
	LOG(INFO) << "Got memory " << d_mapping->get_address() << " size: " <<d_mapping->get_size();

	if (d_mapping->get_size() < sizeof(Header) ) {
		throw std::runtime_error("Invalid Shared Memory size");
	}
	d_header = static_cast<Header*>(d_mapping->get_address());
	if ( d_mapping->get_size() < NeededSize(d_header->Width,d_header->Height) ) {
		throw std::runtime_error("Insufficient Shared Memory size");
	}
	LOG(INFO) << d_header->Width << "x" << d_header->Height;
	d_detections = (Detection*)(d_header + 1);
	d_image = cv::Mat(d_header->Height,d_header->Width,CV_8U,d_detections + DETECTION_SIZE);
}

InterprocessBuffer::Detection * InterprocessBuffer::Detections() {
	return d_detections;
}

size_t * InterprocessBuffer::DetectionsSize() {
	return &(d_header->Size);
}

cv::Mat & InterprocessBuffer::Image() {
	return d_image;
}

uint64_t * InterprocessBuffer::TimestampIn() {
	return &(d_header->TimestampIn);
}

uint64_t *  InterprocessBuffer::TimestampOut() {
	return &(d_header->TimestampOut);
}

const DetectionConfig & InterprocessBuffer::Config() const {
	return d_header->Config;
}
