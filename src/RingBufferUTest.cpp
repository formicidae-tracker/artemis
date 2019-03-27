#include "RingBufferUTest.h"

#include <thread>
#include <condition_variable>


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
	EXPECT_THROW({d_consumer->Pop();},RB::EmptyException);
	for ( size_t i = 0 ; i < RB::Size; ++i ) {

		EXPECT_NO_THROW({
				d_producer->Tail() = i;
				d_producer->Push();
			});
		EXPECT_EQ( (i + 1) == RB::Size, d_producer->Full());
	}
	//Cannot push on a Full one
	EXPECT_TRUE(d_producer->Full());
	EXPECT_THROW({
			d_producer->Push();
		},RB::FullException);

	//we can pop safely
	for ( size_t i = 0; i < RB::Size; ++i ) {
		EXPECT_FALSE(d_consumer->Empty());
		int res;
		EXPECT_NO_THROW({
				res = d_consumer->Head();
				d_consumer->Pop();
			});
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
			ASSERT_NO_THROW({
					d_producer->Tail() = id;
					d_producer->Push();
				});
			++id;
		}

		for(size_t j = 0; j < nbPush; ++j) {
			int res;
			ASSERT_NO_THROW({
					res = d_consumer->Head();
					d_consumer->Pop();
				});
			ASSERT_EQ(id - nbPush + j, res);
		}
	}
}

TEST_F(RingBufferUTest,AreThreadSafe) {
	size_t nbOperation = RB::Size * 100;



	std::thread t([nbOperation](RB::Producer::Ptr p) {
			for(size_t i = 0; i < nbOperation; ++i) {
				bool done = false;
				while(!false) {
					try {
						p->Tail() = i;
						p->Push();
						break;
					} catch( const RB::FullException &) {
					}

					usleep(10);
				}
			}
		},
		std::move(d_producer));

	for( size_t i = 0 ; i < nbOperation; ++i) {
		int res;
		while(true) {
			try {
				res = d_consumer->Head();
				d_consumer->Pop();
				break;
			} catch ( const RB::EmptyException &) {
			}
			usleep(5);
		}
		EXPECT_EQ(res,i);
	}

	t.join();

}
