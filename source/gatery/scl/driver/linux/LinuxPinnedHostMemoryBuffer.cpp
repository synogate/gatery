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

#include "LinuxPinnedHostMemoryBuffer.h"

namespace gtry::scl::driver::lnx {

LinuxPinnedHostMemoryBuffer::LinuxPinnedHostMemoryBuffer(LinuxPinnedHostMemoryBufferFactory &factory, PinnedMemory &&pinnedMemory)
			 : m_factory(factory), m_pinnedMemory(std::move(pinnedMemory))
{
	m_pageSize = m_pinnedMemory.pageSize();
}

LinuxPinnedHostMemoryBuffer::~LinuxPinnedHostMemoryBuffer()
{
	m_factory.returnPinnedMemory(std::move(m_pinnedMemory));
}

PhysicalAddr LinuxPinnedHostMemoryBuffer::physicalPageStart(size_t page) const
{
	return m_pinnedMemory.userToPhysical((void*)(m_pinnedMemory.userSpaceBuffer().data() + page * m_pageSize));
}

std::unique_ptr<MemoryBuffer> LinuxPinnedHostMemoryBufferFactory::allocate(uint64_t bytes)
{
	auto &poolElems = m_pool[bytes];
	if (poolElems.empty())
		return std::make_unique<LinuxPinnedHostMemoryBuffer>(*this, PinnedMemory(m_addrTranslator, bytes));
	else {
		auto res = std::make_unique<LinuxPinnedHostMemoryBuffer>(*this, std::move(poolElems.back()));
		poolElems.pop_back();
		return res;
	}
}

void LinuxPinnedHostMemoryBufferFactory::returnPinnedMemory(PinnedMemory &&pinnedMemory)
{
	m_pool[pinnedMemory.size()].push_back(std::move(pinnedMemory));
}

}
