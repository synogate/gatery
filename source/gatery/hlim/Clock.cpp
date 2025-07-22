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
#include "Clock.h"

#include "../utils/Exceptions.h"
#include "../utils/Preprocessor.h"

#include "Node.h"
#include "coreNodes/Node_Signal2Clk.h"
#include "coreNodes/Node_Signal2Rst.h"
#include "supportNodes/Node_ExportOverride.h"

namespace gtry::hlim {
	
Clock::Clock()
{
	m_name = "clk";
	m_resetName = "reset";
	m_triggerEvent = TriggerEvent::RISING;
	m_parentClock = nullptr;
}


Clock::~Clock()
{
	while (!m_clockedNodes.empty())
		m_clockedNodes.anyOrder().begin()->node->detachClock(m_clockedNodes.anyOrder().begin()->port);
}
/*
Clock &Clock::createClockDivider(ClockRational frequencyDivider, ClockRational phaseShift = 0)
{
	assert(false);
	return 
}
*/

std::vector<const Clock*> Clock::getPathToRoot() const
{
	std::vector<const Clock*> result;
	auto *ptr = this;
	while (ptr != nullptr) {
		result.push_back(ptr);
		ptr = ptr->getParentClock();
	}
	return result;
}

std::pair<std::vector<const Clock*>, std::vector<const Clock*>> Clock::getRelationTo(const Clock &other) const
{
	auto myPath = getPathToRoot();
	auto otherPath = other.getPathToRoot();

	while (!myPath.empty() && !otherPath.empty() && myPath.back() == otherPath.back()) {
		myPath.pop_back();
		otherPath.pop_back();
	}
	return { std::move(myPath), std::move(otherPath) };
}

PhaseShift Clock::getPhaseShiftRelativeTo(const Clock &other) const
{
	auto relation = getRelationTo(other);
	PhaseShift shift;
	shift.shift = 0;
	shift.shiftStable = true;

	for (const auto *clk : relation.first) {
		shift.shift -= clk->getPhaseShift().shift * clk->absoluteFrequency();
		shift.shiftStable &= clk->getPhaseShift().shiftStable;
	}

	if (relation.first.back()->getParentClock() == nullptr)
		shift.shiftStable = false;

	for (const auto *clk : relation.second) {
		shift.shift -= clk->getPhaseShift().shift * clk->absoluteFrequency();
		shift.shiftStable &= clk->getPhaseShift().shiftStable;
	}

	return shift;
}

std::unique_ptr<Clock> Clock::cloneUnconnected(Clock *newParent)
{
	std::unique_ptr<Clock> res = allocateClone(newParent);
	res->m_name = m_name;
	res->m_resetName = m_resetName;
	res->m_triggerEvent = m_triggerEvent;
	res->m_registerAttributes = m_registerAttributes;
	res->m_phaseShift = m_phaseShift;
	return res;
}


ClockRational Clock::getMinResetTime() const
{
	ClockRational res = m_minResetTime;

	if (m_registerAttributes.resetType == RegisterAttributes::ResetType::ASYNCHRONOUS && !m_clockedNodes.empty())
		res = std::max(res, ClockRational{1,1}/absoluteFrequency());

	for (auto d : m_derivedClocks)
		res = std::max(res, d->getMinResetTime());
	return res;
}

size_t Clock::getMinResetCycles() const
{
	size_t res = m_minResetCycles;

	if (m_registerAttributes.resetType == RegisterAttributes::ResetType::SYNCHRONOUS && !m_clockedNodes.empty())
		res = std::max<size_t>(res, 1);

	for (const DerivedClock* d : m_derivedClocks) {
		ClockRational cycles = d->getMinResetCycles() / d->getFrequencyMuliplier();
		res = std::max(res, ceil(cycles));
	}
	if (m_phaseShift.shift != ClockRational{0})
		res += 1;
	return res;
}

const std::vector<NodePort>& Clock::getClockedNodes() const
{
	if (m_clockedNodesCache.empty())
	{
		m_clockedNodesCache.reserve(m_clockedNodes.size());
		std::copy(m_clockedNodes.anyOrder().begin(), m_clockedNodes.anyOrder().end(), std::back_inserter(m_clockedNodesCache));
		std::sort(m_clockedNodesCache.begin(), m_clockedNodesCache.end(), utils::StableCompare<NodePort>());
	}
	return m_clockedNodesCache;
}

bool Clock::inheritsClockPinSource() const
{
	if (m_parentClock == nullptr)
		return false;

	if (!isSelfDriven(true, true) || !isSelfDriven(false, true))
		return false;

	if (m_parentClock->getName() != getName() ||
		m_parentClock->absoluteFrequency() != absoluteFrequency() ||
		!m_phaseShift.shiftStable || m_phaseShift.shift != ClockRational{0})
		return false;

	return true;
}

Clock *Clock::getClockPinSource()
{
	if (inheritsClockPinSource())
		return m_parentClock->getClockPinSource();
	else
		return this;
}

const Clock *Clock::getClockPinSource() const
{
	if (inheritsClockPinSource())
		return m_parentClock->getClockPinSource();
	else
		return this;
}

bool Clock::inheritsResetPinSource() const
{
	if (getRegAttribs().resetType == RegisterAttributes::ResetType::NONE)
		return false;

	if (m_parentClock == nullptr)
		return false;

	if (!isSelfDriven(true, false) || !isSelfDriven(false, false))
		return false;

	if (m_parentClock->getResetName() != getResetName())
		return false;

	return true;
}

Clock *Clock::getResetPinSource()
{
	if (getRegAttribs().resetType == RegisterAttributes::ResetType::NONE)
		return nullptr;

	if (inheritsResetPinSource())
		return m_parentClock->getResetPinSource();
	else
		return this;
}

void Clock::setLogicClockDriver(Node_Signal2Clk *driver)
{
	if (m_clockDriver != nullptr)
		m_clockDriver->setClock(nullptr);
	m_clockDriver = driver;
	m_clockDriver->setClock(this);
}

void Clock::setLogicResetDriver(Node_Signal2Rst *driver)
{
	if (m_resetDriver != nullptr)
		m_resetDriver->setClock(nullptr);
	m_resetDriver = driver;
	m_resetDriver->setClock(this);
}

bool Clock::isSelfDriven(bool simulation, bool clk) const
{
	return getLogicDriver(simulation, clk).node == nullptr;
}

NodePort Clock::getLogicDriver(bool simulation, bool clk) const
{
	NodePort driver;
	if (clk) {
		if (m_clockDriver != nullptr)
			driver = m_clockDriver->getNonForwardingDriver(0);
	} else {
		if (m_resetDriver != nullptr)
			driver = m_resetDriver->getNonForwardingDriver(0);
	}

	utils::UnstableSet<NodePort> alreadyVisited;

	while (driver.node != nullptr) {
		if (alreadyVisited.contains(driver)) return {};
		alreadyVisited.insert(driver);

		auto *expOverride = dynamic_cast<Node_ExportOverride*>(driver.node);
		if (expOverride) {
			if (simulation)
				driver = expOverride->getNonForwardingDriver(Node_ExportOverride::Inputs::SIM_INPUT);
			else
				driver = expOverride->getNonForwardingDriver(Node_ExportOverride::Inputs::EXP_INPUT);
		} else
			return driver;
	}

	return {};
}


RootClock::RootClock(std::string name, ClockRational frequency) : m_frequency(frequency)
{
	m_name = std::move(name);
}


PhaseShift RootClock::absolutePhaseShift() const
{
	return {
		m_phaseShift.shift / m_frequency,
		m_phaseShift.shiftStable
	};
}


std::unique_ptr<Clock> RootClock::cloneUnconnected(Clock *newParent)
{
	HCL_ASSERT(newParent == nullptr);
	std::unique_ptr<Clock> res = Clock::cloneUnconnected(newParent);
	((RootClock*)res.get())->m_frequency = m_frequency;
	return res;
}

std::unique_ptr<Clock> RootClock::allocateClone(Clock *newParent)
{
	return std::unique_ptr<Clock>(new RootClock(m_name, m_frequency));
}

	
DerivedClock::DerivedClock(Clock *parentClock)
{
	m_parentClock = parentClock;
	m_parentRelativeMultiplicator = 1;
	
	m_name = m_parentClock->getName();
	
	m_resetName = m_parentClock->getResetName();
	m_triggerEvent = m_parentClock->getTriggerEvent();

	m_registerAttributes = m_parentClock->getRegAttribs();
	m_parentClock->addDerivedClock(this);
}

	
ClockRational DerivedClock::absoluteFrequency() const
{
	return m_parentClock->absoluteFrequency() * m_parentRelativeMultiplicator;
}

PhaseShift DerivedClock::absolutePhaseShift() const
{
	auto parentShift = m_parentClock->absolutePhaseShift();
	return {
		m_phaseShift.shift / absoluteFrequency() + parentShift.shift,
		m_phaseShift.shiftStable && parentShift.shiftStable
	};
}


std::unique_ptr<Clock> DerivedClock::cloneUnconnected(Clock *newParent)
{
	std::unique_ptr<Clock> res = Clock::cloneUnconnected(newParent);
	((DerivedClock*)res.get())->m_parentRelativeMultiplicator = m_parentRelativeMultiplicator;
	return res;
}

std::unique_ptr<Clock> DerivedClock::allocateClone(Clock *newParent)
{
	return std::unique_ptr<Clock>(new DerivedClock(newParent));
}



}
