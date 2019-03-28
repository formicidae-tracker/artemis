#pragma once

#include <gtest/gtest.h>

#include "EventManager.h"
#include <asio/io_service.hpp>


class ProcessManagerUTest : public ::testing::Test {
protected:
	void SetUp();
	void TearDown();


	asio::io_service d_service;
	EventManager::Ptr d_events;

};
