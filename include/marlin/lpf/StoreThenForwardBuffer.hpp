#ifndef MARLIN_LPF_STFB_HPP
#define MARLIN_LPF_STFB_HPP

#include <marlin/net/Buffer.hpp>

namespace marlin {
namespace lpf {

class StoreThenForwardBuffer {
	char *buf = nullptr;
	uint64_t length = 0;
	uint64_t size = 0;

public:
	~StoreThenForwardBuffer() {
		delete[] buf;
	}

	uint16_t id = 0;

	template<typename Delegate>
	int did_recv_bytes(
		Delegate &delegate,
		net::Buffer &&bytes
	) {
		if(bytes.size() == 0) return 0;

		if(buf == nullptr) { // Read length
			if(bytes.size() + size < 8) { // Partial length
				for(size_t i = 0; i < bytes.size(); i++) {
					length = (length << 8) | (uint8_t)bytes.data()[i];
				}
				size += bytes.size();
			} else { // Full length
				for(size_t i = 0; i < 8 - size; i++) {
					length = (length << 8) | (uint8_t)bytes.data()[i];
				}
				bytes.cover(8 - size);

				if(length > 5000000) { // Abort on big message, DoS prevention
					SPDLOG_ERROR("Message too big: {}", length);
					return -1;
				}

				// Prepare to process message
				buf = new char[length];
				size = 0;

				// Process remaining bytes
				did_recv_bytes(delegate, std::move(bytes));
			}
		} else { // Read message
			if(bytes.size() + size < length) { // Partial message
				std::memcpy(buf + size, bytes.data(), bytes.size());
				size += bytes.size();
			} else { // Full message
				std::memcpy(buf + size, bytes.data(), length - size);
				bytes.cover(length - size);

				delegate.did_recv_stf_message(id, net::Buffer(buf, length));

				// Prepare to process length
				buf = nullptr;
				size = 0;
				length = 0;

				// Process remaining bytes
				did_recv_bytes(delegate, std::move(bytes));
			}
		}

		return 0;
	}
};

} // namespace lpf
} // namespace marlin

#endif // MARLIN_LPF_STFB_HPP
