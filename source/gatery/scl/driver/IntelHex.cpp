#include "gatery/scl_pch.h"
#include "IntelHex.h"

std::ostream& gtry::scl::operator<<(std::ostream& o, const IntelHexRecord& r)
{
	assert(r.data.size() < 256);

	size_t checksum = 0;
	auto putByte = [&](size_t b) {
		checksum += b;
		o << std::setw(2) << (b & 0xFF);
	};

	o << std::setw(0) << r.comment << ':';
	o << std::hex << std::setfill('0');
	putByte(r.data.size());
	putByte(r.offset >> 8);
	putByte(r.offset & 0xFF);
	putByte(r.type);

	for (uint8_t c : r.data)
		putByte(c);
	o << (~checksum + 1 & 0xFF) << '\n';

	return o;
}
