#pragma once

#include <tbb/concurrent_queue.h>
#include <memory>

namespace fort {
namespace artemis {

template<typename T>
class ObjectPool {
public:
	ObjectPool()
		: d_pool(std::make_shared<tbb::concurrent_bounded_queue<TPtr>>()) {
	}

	template<class ...Us> void Reserve(size_t N, Us... args) {
		std::vector<TPtr> reserved;
		for ( size_t i = 0; i < N; ++i ) {
			reserved.push_back(Get(args...));
		}
	}

	template<class ...Us> std::shared_ptr<T> Get(Us... args) {
		std::shared_ptr<T> obj;
		if ( d_pool->try_pop(obj) == false ) {
			obj = std::make_shared<T>(args...);
		}
		return TPtr(obj.get(),
		            [this,obj](T*){
			            d_pool->push(obj);
		            });
	}

private:
	typedef std::shared_ptr<T> TPtr;

	std::shared_ptr<tbb::concurrent_bounded_queue<TPtr>> d_pool;
};

} // namespace artemis
} // namespace fort
