#include "ObjectPoolUTest.hpp"

#include "ObjectPool.hpp"


namespace fort {
namespace artemis {

class Object {
public:
	Object() : d_value(0) {
		++s_totalAllocated;
		++s_currentlyAllocated;
	}
	Object(int value) : d_value(value) {
		++s_totalAllocated;
		++s_currentlyAllocated;
	}

	~Object() {
		--s_currentlyAllocated;

	}

	int Value() const {
		return d_value;
	}

	static size_t CurrentlyAllocated() {
		return s_currentlyAllocated;
	}

	static size_t TotalAllocated() {
		return s_currentlyAllocated;
	}


private :
	static size_t s_totalAllocated;
	static size_t s_currentlyAllocated;
	int d_value;
};

size_t Object::s_totalAllocated = 0;
size_t Object::s_currentlyAllocated = 0;

TEST_F(ObjectPoolUTest,CanReserve) {
	{
		ObjectPool<Object> pool;

		EXPECT_EQ(Object::CurrentlyAllocated(),0);
		pool.Reserve(4,42);
		EXPECT_EQ(Object::CurrentlyAllocated(),4);
		std::vector<std::shared_ptr<Object>> objects;
		for ( size_t i = 0; i < 4; ++i ) {
			objects.push_back(pool.Get());
			EXPECT_EQ(objects.back()->Value(),42);
		}
	}
	EXPECT_EQ(Object::CurrentlyAllocated(),0);
}

TEST_F(ObjectPoolUTest,IsCopyable) {
	ObjectPool<Object> a;
	auto o1 = a.Get(1);
	ObjectPool b = a;
	auto o2 = b.Get(2);
	o2.reset();
	EXPECT_EQ(Object::CurrentlyAllocated(),2);

	auto o3 = a.Get(3);
	// reallocated from b pool. goes back to pool a and b
	EXPECT_EQ(o3->Value(),2);
	EXPECT_EQ(Object::CurrentlyAllocated(),2);

}

} // namespace artemis
} // namespace fort
