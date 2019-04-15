#pragma once

#include <opencv2/core.hpp>

namespace maytags {

class ComponentConnecter {
public :
	void ConnectComponent(const cv::Mat & image);

	uint32_t GetRepresentative(uint32_t);
	uint32_t GetSize(uint32_t);

	void PrintDebug(const cv::Size & ,cv::Mat & );
private:
	std::vector<uint32_t> d_parent,d_size;

	uint32_t Connect(uint32_t a, uint32_t b);
};

} // namespace maytags


#define REPRESENTATIVE_ID(x,y,w) ( (y) * (w) + (x) )
