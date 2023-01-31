#ifndef RING_BUFFER
#define RING_BUFFER

#include <stdio.h>
#include <stdlib.h>

#include <queue>
#include <mutex>

#include "error_define.h"

namespace am {

	template <typename T>
	struct RingFrame {
		T type;
		int len;
	};

	template <typename T>
	class RingBuffer
	{
	public:
		RingBuffer(unsigned int size = 1920 * 1080 * 4 * 10)
		{
			size_ = size;
			head_ = tail_ = 0;

			buf_ = new uint8_t[size];
		}
		~RingBuffer()
		{
			if (buf_)
				delete[] buf_;
		}

		void put(const void* data, int len, const T& type)
		{
			std::lock_guard<std::mutex> locker(lock_);

			if (head_ + len <= size_) {
				memcpy(buf_ + head_, data, len);

				head_ += len;
			}
			else if (head_ + len > size_) {
				int remain = len - (size_ - head_);
				if (len - remain > 0)
					memcpy(buf_ + head_, data, len - remain);

				if (remain > 0)
					memcpy(buf_, (unsigned char*)data + len - remain, remain);

				head_= remain;
			}

			struct RingFrame<T> frame;
			frame.len = len;
			frame.type = type;

			frames_.push(frame);

		}

		int get(void* data, int len, T& type)
		{
			std::lock_guard<std::mutex> locker(lock_);

			int retLen = 0;

			if (frames_.size() <= 0) {
				retLen = 0;
				return retLen;
			}

			struct RingFrame<T> frame = frames_.front();
			frames_.pop();

			if (frame.len > len) {
				std::cout << "ringbuff::get need larger buffer" << std::endl;
				return 0;
			}

			type = frame.type;

			retLen = frame.len;

			if (tail_ + frame.len <= size_) {

				memcpy(data, buf_ + tail_, frame.len);

				tail_ += frame.len;
			}
			else {
				int remain = frame.len - (size_ - tail_);

				if (frame.len - remain > 0)
					memcpy(data, buf_ + tail_, frame.len - remain);

				if (remain > 0)
					memcpy((unsigned char*)data + frame.len - remain, buf_, remain);

				tail_ = remain;
			}

			return retLen;
		}

	private:
		std::queue<RingFrame<T>> frames_;
		unsigned int size_, head_, tail_;

		uint8_t* buf_;

		std::mutex lock_;
	};

}
#endif
