#pragma once

#include "Task.hpp"

namespace fort {
namespace artemis {

class UserInterfaceTask : public Task {
public:
	virtual ~UserInterfaceTask();

	void Run() override;

};

} // namespace artemis
} // namespace fort
