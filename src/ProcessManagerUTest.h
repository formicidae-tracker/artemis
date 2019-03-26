#pragma once

#include <gtest/gtest.h>

#include "EventManager.h"

class ProcessManagerUTest : public ::testing::Test {
protected:
	void SetUp();
	void TearDown();


	EventManager::Ptr d_manager;

};
