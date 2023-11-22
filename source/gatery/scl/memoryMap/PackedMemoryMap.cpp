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

#include "PackedMemoryMap.h"

#include <ranges>

namespace gtry::scl
{

void PackedMemoryMap::ro(const BVec& value, RegDesc desc)
{
	HCL_DESIGHNCHECK_HINT(!m_alreadyPacked, "All signals must be added to the memory map before computing the packed address map!");
	desc.flags = F_READ;
	if (!m_scopeStack.empty()) desc.scope = m_scopeStack.back();

	m_registeredSignals.emplace_back(value, {}, std::move(desc));
}

Bit PackedMemoryMap::rw(BVec& value, RegDesc desc)
{
	HCL_DESIGHNCHECK_HINT(!m_alreadyPacked, "All signals must be added to the memory map before computing the packed address map!");
	desc.flags = F_READ | F_WRITE;
	if (!m_scopeStack.empty()) desc.scope = m_scopeStack.back();

	m_registeredSignals.emplace_back(value, {}, std::move(desc));
	value = m_registeredSignals.back().signal;
	return m_registeredSignals.back().onWrite;
}

void PackedMemoryMap::enterScope(std::string scope) override
{
	if (!m_scopeStack.empty())
		scope = m_scopeStack.back() + '.' + scope;
	m_scopeStack.push_back(scope);
}

void PackedMemoryMap::leaveScope() override
{
	m_scopeStack.pop_back();
}

void PackedMemoryMap::pack(BitWidth registerWidth)
{
	HCL_DESIGHNCHECK_HINT(!m_alreadyPacked, "Memory map can only be packed once!");
	m_alreadyPacked = true;

	// todo: pack properly
	m_physicalRegisters.resize(m_registeredSignals.size());
	for (auto &[signal, idx] : std::views::enumerate(m_registeredSignals)) {
		m_physicalRegisters[idx].desc = signal.desc;

		m_physicalRegisters[idx].signal = signal.signal;
		signal.signal = m_physicalRegisters[idx].signal;
		signal.onWrite = m_physicalRegisters[idx].onWrite;
	}
}


}