/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2021 Michael Offel, Andreas Ley

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
#include "gatery/pch.h"

#include "NodePort.h"

#include "SignalDelay.h"
#include "Subnet.h"
#include "../utils/Range.h"
#include "Node.h"

#include "TopologicalSort.h"


namespace gtry::hlim {

void SignalDelay::compute(const Subnet &subnet)
{
	allocate(subnet);
	zero();

	TopologicalSort sorter;
	const auto &sorted = sorter.sort(subnet);

	for (auto n : sorted)
		n->estimateSignalDelay(*this);
}

void SignalDelay::allocate(const Subnet &subnet)
{
	m_outputToBitDelays.clear();

	utils::StableMap<NodePort, std::pair<size_t, size_t>> allocations;

	size_t totalSize = 0;
	for (auto n : subnet.getNodes())
		for (auto i : utils::Range(n->getNumOutputPorts())) {
			NodePort np{.node=n, .port=i};

			if (outputIsDependency(np)) continue;

			size_t width = getOutputWidth(np);
			allocations[np] = {totalSize, width};
			totalSize += width;
		}

	m_delays.resize(totalSize);
	for (auto &alloc : allocations)
		m_outputToBitDelays[alloc.first] = std::span<float>(m_delays.data() + alloc.second.first, alloc.second.second);
}

void SignalDelay::zero()
{
	memset(m_delays.data(), 0, m_delays.size() * sizeof(float));
}

std::span<float> SignalDelay::getDelay(const NodePort &np)
{
	auto it = m_outputToBitDelays.find(np);
	if (it != m_outputToBitDelays.end())
		return it->second; 

	size_t width = 0;
	if (np.node != nullptr)
		width = getOutputWidth(np);

	m_zeros.resize(std::max(m_zeros.size(), width));
	return std::span<float>(m_zeros.data(), width);
}

std::span<const float> SignalDelay::getDelay(const NodePort &np) const
{
	auto it = m_outputToBitDelays.find(np);
	if (it != m_outputToBitDelays.end())
		return it->second; 

	size_t width = 0;
	if (np.node != nullptr)
		width = getOutputWidth(np);

	m_zeros.resize(std::max(m_zeros.size(), width));
	return std::span<const float>(m_zeros.data(), width);
}

}
