#pragma once

#include <memory>
#include <atomic>
#include <stdexcept>

// Single Producer, Single Consumer, lock free, thread safe, ring buffer
// @T the type of object to buffer
// @SizeT maximal number of T object that could be stored
//
// This templated class implements a lock free, thread safe ring
// buffer. A ring buffer is used for buffering data continuously
// without memory allocation. This implementation is lock-free, and is
// guaranted to be thread safe in a single producer / single consumer
// scenario.
//
// To enforce the proper use of this class, the <Create> creates two
// entry point, a <Producer> and a <Consumer> interfaces as
// std::unique_ptr to ensure that only one thread can access one or
// the other interface. The constrcutor of a <RingBuffer> is private.
//
// On overflow condition, this <RingBuffer> will drop new data first
template <typename T, size_t SizeT>
class RingBuffer {
private :
	typedef std::shared_ptr<RingBuffer> Ptr;
public :
	// The Size of the <RingBuffer>
	const static size_t Size = SizeT;
	// Accessor to add data
	//
	// A <Producer> expose the <Push> method of a <RingBuffer> to add
	// new data to the buffer.
	class Producer {
	public :
		// Pointer to a <Producer>
		typedef std::unique_ptr<Producer> Ptr;

		// Standard Destructor
		~Producer();

		// Adds a new value to the buffer
		// @value the value to add
		//
		// @return true if the value could be added, false if the
		//         buffer is full (value is then dropped)
		bool Push(const T & value);

		// Tests if the <RingBuffer> is full
		//
		// @return true if it is currently full.
		bool Full();

	private :
		Producer(const RingBuffer::Ptr & buffer);

		RingBuffer::Ptr d_buffer;
		friend class RingBuffer;
	};

	// Accessor to pop data
	//
	// A <Consumer> expose the <Pop> method of a <RingBuffer> to
	// access and remove data from the buffer.
	class Consumer {
	public :
		// Pointer to a <Consumer>
		typedef std::unique_ptr<Consumer> Ptr;

		//Standard destructor
		~Consumer();

		// Pops a value from the buffer
		// @value reference that will hold the popped data
		//
		// @return true if a value was popped (and <value>
		//         updated). Otherwise <value> is unmodified.
		bool Pop(T & value);

		// Tests if the buffer is empty
		//
		// @return true if the buffer is empty.
		bool Empty();

	private :
		Consumer(const RingBuffer::Ptr & buffer);

		RingBuffer::Ptr d_buffer;
		friend class RingBuffer;
	};

	// Pointer to a <Producer>
	typedef typename Producer::Ptr ProducerPtr;
	// Pointer to a <Consumer>
	typedef typename Consumer::Ptr ConsumerPtr;

	// Creates  a new <RingBuffer>
	//
	// <RingBuffer> does not have a public constructor. User needs to
	// uses this function to get two entry point to two accessor, one
	// for consuming and one for producing data. This is to enforce
	// the right use case of this <RingBuffer> implementation that
	// suffers the ABA problem if used in either multiple consumer or
	// multiple producer scenarii. User are intended to use these
	// pointers and not reference to it.
	//
	//
	// @return a pair of a <Producer::Ptr> and <Consumer::Ptr>
	//         refering to the same <RingBuffer>
	static std::pair<ProducerPtr,ConsumerPtr> Create();

	// Standard destructor.
	~RingBuffer();

private :
	friend class Producer;
	friend class Consumer;

	const static size_t Capicity = Size + 1;
	RingBuffer();


	bool Push(const T & value);
	bool Pop(T & value);

	bool WasEmpty();
	bool WasFull();

	size_t Increment(size_t value);

	std::atomic<size_t> d_tail,d_head;
	T d_data[Capicity];

};


template <typename T, size_t SizeT>
inline std::pair<typename RingBuffer<T,SizeT>::ProducerPtr,
                 typename RingBuffer<T,SizeT>::ConsumerPtr>
RingBuffer<T,SizeT>::Create() {
	RingBuffer<T,SizeT>::Ptr buffer = RingBuffer<T,SizeT>::Ptr(new RingBuffer<T,SizeT>());
	return std::make_pair(ProducerPtr(new Producer(buffer)),
	                      ConsumerPtr(new Consumer(buffer)));
}

template <typename T, size_t SizeT>
inline RingBuffer<T,SizeT>::~RingBuffer(){}


template <typename T, size_t SizeT>
inline RingBuffer<T,SizeT>::RingBuffer()
	: d_head(0)
	, d_tail(0) {
	static_assert(Capicity != 0, "We need Size to be smaller std::numeric_limits<size_t>::max()");
	if (  ! d_head.is_lock_free() || ! d_tail.is_lock_free() ) {
		throw std::runtime_error("std::atomic<size_t> is not lock free on this platform");
	}
}

template <typename T, size_t SizeT>
inline bool RingBuffer<T,SizeT>::Push(const T & value) {
	const auto tail = d_tail.load();
	const auto next_tail(Increment(tail));
	if (next_tail == d_head.load() ){
		// we are full
		return false;
	}
	//we store and save the increment
	d_data[tail] = value;
	d_tail.store(next_tail);
	return true;

}

template <typename T, size_t SizeT>
inline bool RingBuffer<T,SizeT>::Pop(T & value) {
	const auto head = d_head.load();
	if ( head == d_tail.load() ) {
		//we are empty
		return false;
	}

	value = d_data[head];
	d_head.store(Increment(head));
	return true;
}


template <typename T, size_t SizeT>
inline bool RingBuffer<T,SizeT>::WasEmpty() {
	return ( d_head.load() == d_tail.load() );
}

template <typename T, size_t SizeT>
inline bool RingBuffer<T,SizeT>::WasFull() {
	return ( Increment(d_tail.load()) == d_head.load() );
}


template <typename T, size_t SizeT>
inline size_t RingBuffer<T,SizeT>::Increment(size_t value) {
	return (value + 1) % Capicity;
}

template <typename T, size_t SizeT>
inline RingBuffer<T,SizeT>::Producer::~Producer(){}


template <typename T, size_t SizeT>
inline bool RingBuffer<T,SizeT>::Producer::Push(const T & value){
	return d_buffer->Push(value);
}

template <typename T, size_t SizeT>
inline bool RingBuffer<T,SizeT>::Producer::Full(){
	return d_buffer->WasFull();
}

template <typename T, size_t SizeT>
inline RingBuffer<T,SizeT>::Producer::Producer(const RingBuffer<T,SizeT>::Ptr & buffer)
	: d_buffer(buffer) {
}

template <typename T, size_t SizeT>
inline RingBuffer<T,SizeT>::Consumer::~Consumer(){}


template <typename T, size_t SizeT>
inline bool RingBuffer<T,SizeT>::Consumer::Pop(T & value){
	return d_buffer->Pop(value);
}

template <typename T, size_t SizeT>
inline bool RingBuffer<T,SizeT>::Consumer::Empty(){
	return d_buffer->WasEmpty();
}

template <typename T, size_t SizeT>
inline RingBuffer<T,SizeT>::Consumer::Consumer(const RingBuffer<T,SizeT>::Ptr & buffer)
	: d_buffer(buffer) {
}
