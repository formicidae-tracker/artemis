#pragma once

#include <opencv2/core.hpp>

namespace maytags {

class ComponentConnecter {
public :
	void ConnectComponent(const cv::Mat & image);

	size_t GetRepresentative(size_t);
	size_t GetSize(size_t);

	void PrintDebug(const cv::Size & ,cv::Mat & );
private:
	std::vector<size_t> d_parent,d_size;

	size_t Connect(size_t a, size_t b);
};

} // namespace maytags
