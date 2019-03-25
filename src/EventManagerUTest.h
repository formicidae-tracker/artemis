#pragma once

#include <gtest/gtest.h>

#include "EventManager.h"

class EventManagerUTest : public ::testing::Test {
protected:
	void SetUp();
	void TearDown();


	EventManager::Ptr d_manager;
};
