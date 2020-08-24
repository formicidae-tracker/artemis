#pragma once

#include "Task.hpp"

namespace fort {
namespace artemis {

class VideoOutputTask : public Task {
public:
	virtual ~VideoOutputTask();

	void Run() override;

};

} // namespace artemis
} // namespace fort
