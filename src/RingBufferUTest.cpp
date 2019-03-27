#include "RingBufferUTest.h"

#include <thread>

void RingBufferUTest::SetUp() {
	auto res = RB::Create();
	d_producer = std::move(res.first);
	d_consumer = std::move(res.second);
}

void RingBufferUTest::TearDown() {
	d_producer = RB::Producer::Ptr();
	d_consumer = RB::Consumer::Ptr();
}


TEST_F(RingBufferUTest,HaveALimitedSize) {
	EXPECT_TRUE(d_consumer->Empty());
	int foo;
	EXPECT_FALSE(d_consumer->Pop(foo));
	for ( size_t i = 0 ; i < RB::Size; ++i ) {
		EXPECT_TRUE(d_producer->Push(i));
		EXPECT_EQ( (i + 1) == RB::Size, d_producer->Full());
	}
	//Cannot push on a Full one
	EXPECT_TRUE(d_producer->Full());
	EXPECT_FALSE(d_producer->Push(RB::Size));

	//we can pop safely
	for ( size_t i = 0; i < RB::Size; ++i ) {
		EXPECT_FALSE(d_consumer->Empty());
		int res;
		EXPECT_TRUE(d_consumer->Pop(res));
		// preserves order
		EXPECT_EQ(i,res);
	}
	EXPECT_TRUE(d_consumer->Empty());
}

TEST_F(RingBufferUTest,CanSafelyOverlapSize) {
	int id(0);
	for(size_t i = 0; i < RB::Size * 1000; ++i) {
		//we do burst of 1-4 push/pull
		size_t nbPush = ((double)rand()) / ((double)RAND_MAX) * RB::Size;

		for(size_t j = 0 ; j < nbPush; ++j) {
			ASSERT_TRUE(d_producer->Push(id));
			++id;
		}

		for(size_t j = 0; j < nbPush; ++j) {
			int res;
			ASSERT_TRUE(d_consumer->Pop(res));
			ASSERT_EQ(id - nbPush + j, res);
		}
	}
}

TEST_F(RingBufferUTest,AreThreadSafe) {
	size_t nbOperation = RB::Size * 100;

	std::thread t([nbOperation](RB::Producer::Ptr p) {
			for(size_t i = 0; i < nbOperation; ++i) {
				while(true) {
					if(p->Push(i)) {
						break;
					}
					usleep(10);
				}
			}
		},
		std::move(d_producer));

	for( size_t i = 0 ; i < nbOperation; ++i) {
		int res;
		while(true) {
			if ( d_consumer->Pop(res) ) {
				break;
			}
			usleep(5);
		}
		EXPECT_EQ(i,res);
	}

	t.join();

}
