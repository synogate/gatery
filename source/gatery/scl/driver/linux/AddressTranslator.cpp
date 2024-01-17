/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

	Gatery is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 3 of the License, or (at your option) any later version.

	Gatery is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
 * Do not include the regular gatery headers since this is meant to compile stand-alone in driver/userspace application code. 
 */

#include "AddressTranslator.h"

#include <unistd.h>
#include <fcntl.h>

#include <stdexcept>

namespace gtry::scl::driver::lnx {

AddressTranslator::AddressTranslator()
{
	m_pageMapFd = open("/proc/self/pagemap", O_RDONLY);
    if (m_pageMapFd < 0)
		throw std::runtime_error("Could not open pagemap of process of address translation. Missing (root) access rights?");

	m_pageSize = sysconf(_SC_PAGE_SIZE);
}

AddressTranslator::~AddressTranslator()
{
    close(m_pageMapFd);
}

PhysicalAddr AddressTranslator::userToPhysical(void *usrSpaceAddr) const
{
	uint64_t virtualFrameNumber = (uint64_t) usrSpaceAddr / m_pageSize;

	uint64_t data;
    for (size_t numBytesRead = 0; numBytesRead < sizeof(data); ) {
		auto remaining = std::as_writable_bytes(std::span{&data, sizeof(data)}).subspan(numBytesRead, sizeof(data) - numBytesRead);

    	ssize_t res = pread(m_pageMapFd, remaining.data(), remaining.size(), virtualFrameNumber * sizeof(data) + numBytesRead);
		if (res <= 0)
			throw std::runtime_error("An error occured reading from the processes pagemap file for address translation.");

		numBytesRead += res;
	}

    bool filePage = (data >> 61) & 1;
    bool swapped = (data >> 62) & 1;
    bool present = (data >> 63) & 1;

	if (filePage)
		throw std::runtime_error("Could not translate address as its page belongs to a file!");

	if (swapped)
		throw std::runtime_error("Could not translate address as its page is swapped out!");

	if (!present)
		throw std::runtime_error("Could not translate address as its page is not present!");

	uint64_t pageFrameNumber = data & ((1ull << 55) - 1);
	return pageFrameNumber * m_pageSize;
}

}
