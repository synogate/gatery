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

#include "gatery/scl_pch.h"
#include "PackedMemoryMap.h"

#include <ranges>

namespace gtry::scl
{

PackedMemoryMap::PackedMemoryMap(std::string_view name, const CompoundAnnotation *annotation) : m_area("MemoryMap")
{
	m_scope.name = name;
	m_scope.annotation = annotation;
	m_scopeStack.push_back(&m_scope);
}

void PackedMemoryMap::enterScope(std::string_view name, const CompoundAnnotation *annotation)
{
	auto &current = *m_scopeStack.back();

	for (auto &c : current.subScopes)
		if (c.name == name) {
			m_scopeStack.push_back(&c);
			return;
		}

	current.subScopes.push_back(Scope{.name = std::string(name), .annotation = annotation});
	m_scopeStack.push_back(&current.subScopes.back());
}

void PackedMemoryMap::leaveScope()
{
	m_scopeStack.pop_back();
}

void PackedMemoryMap::readable(const ElementarySignal &value, std::string_view name, const CompoundMemberAnnotation *annotation)
{
	auto scope = m_area.enter();

	HCL_DESIGNCHECK_HINT(!m_alreadyPacked, "All signals must be added to the memory map before computing the packed address map!");
	auto &current = *m_scopeStack.back();

	auto *signal = findSignal(current, name);
	if (signal == nullptr) {
		current.registeredSignals.push_back({.name = std::string(name), .annotation = annotation});
		signal = &current.registeredSignals.back();
	}

	signal->readSignal = value.toBVec();
	setName(*signal->readSignal, std::string(name)+"_read");
}

MemoryMap::SelectionHandle PackedMemoryMap::writeable(ElementarySignal &value, std::string_view name, const CompoundMemberAnnotation *annotation)
{
	auto scope = m_area.enter();

	HCL_DESIGNCHECK_HINT(!m_alreadyPacked, "All signals must be added to the memory map before computing the packed address map!");

	auto &current = *m_scopeStack.back();

	auto *signal = findSignal(current, name);
	if (signal == nullptr) {
		current.registeredSignals.push_back({.name = std::string(name), .annotation = annotation});
		signal = &current.registeredSignals.back();
	}

#if 0
	signal->writeSignal = constructFrom(value.toBVec());
	signal->onWrite = Bit{};
	setName(*signal->writeSignal, std::string(name)+"_write");
	value.fromBVec(*signal->writeSignal);

	*signal->writeSignal = reg(*signal->writeSignal, zext(BVec(0), signal->writeSignal->width()));
	setName(*signal->writeSignal, std::string(name)+"_register");
#else
	BVec bvecValue = value.toBVec();
	bvecValue = reg(bvecValue, zext(BVec(0), value.width()));
	setName(bvecValue, std::string(name)+"_register");

	signal->writeSignal = constructFrom(value.toBVec());
	setName(*signal->writeSignal, std::string(name)+"_write");
	value.fromBVec(*signal->writeSignal);
	*signal->writeSignal = bvecValue;

	signal->onWrite = Bit{};
#endif
	return SelectionHandle::SingleSignal(value, *signal->onWrite);
}


void PackedMemoryMap::packRegisters(BitWidth registerWidth)
{
	auto scope = m_area.enter();

	HCL_DESIGNCHECK_HINT(!m_alreadyPacked, "Memory map can only be packed once!");
	m_alreadyPacked = true;

	m_scope.offsetInBits = 0;
	packRegisters(registerWidth, m_scope);
}

void PackedMemoryMap::packRegisters(BitWidth registerWidth, Scope &scope)
{
	scope.physicalDescription = std::make_shared<AddressSpaceDescription>();
	scope.physicalDescription->name = scope.name;
	if (scope.annotation != nullptr) {
		scope.physicalDescription->descShort = scope.annotation->shortDesc;
		scope.physicalDescription->descLong = scope.annotation->longDesc;
	}

	scope.physicalDescription->size = 0_b;
	for (auto & signal : scope.registeredSignals) {
		BitWidth signalWidth;
		if (signal.readSignal)
			signalWidth = signal.readSignal->width();
		else
			signalWidth = signal.writeSignal->width();

		size_t numRegisters = (signalWidth.bits() + registerWidth.bits()-1) / registerWidth.bits();

		Vector<BVec> writeParts;
		writeParts.resize(numRegisters);

		BVec oldWriteSignal;
		if (signal.writeSignal)
			oldWriteSignal = *signal.writeSignal;

		auto description = std::make_shared<AddressSpaceDescription>();
		description->size = signalWidth;
		description->name = signal.name;
		if (signal.annotation != nullptr)
			description->descShort = signal.annotation->shortDesc;

		for (auto i : utils::Range(numRegisters)) {
			size_t startOffset = i * registerWidth.bits();
			BitWidth chunkSize = std::min(registerWidth, signalWidth - i * registerWidth);

			scope.physicalRegisters.push_back({});
			auto &physReg = scope.physicalRegisters.back();
			physReg.offsetInBits = scope.physicalDescription->size.bits() + startOffset;
			if (numRegisters > 1) {
				physReg.description = std::make_shared<AddressSpaceDescription>();
				physReg.description->name = signal.name+"_bits_"+std::to_string(startOffset) + "_to_"+std::to_string(startOffset + chunkSize.bits()-1);
				physReg.description->size = chunkSize;
				if (signal.annotation != nullptr)
					physReg.description->descShort = signal.annotation->shortDesc;
		
				description->children.emplace_back(startOffset, physReg.description);
			} else
				physReg.description = description;

			if (signal.readSignal) {
				physReg.readSignal = (*signal.readSignal)(startOffset, chunkSize);
				setName(*physReg.readSignal, physReg.description->name+"_readOut");
			}
			if (signal.writeSignal) {
				physReg.writeSignal = chunkSize;
				physReg.onWrite = Bit{};

				// Hook write signal in order to default to no-change
				setName(*physReg.writeSignal, physReg.description->name+"_writeIn");
				writeParts[i] = *physReg.writeSignal;
				*physReg.writeSignal = oldWriteSignal(startOffset, chunkSize);
				setName(*physReg.writeSignal, physReg.description->name+"_writeOut");

				// Don't hook the onWrite in order to default to '0'
				*signal.onWrite = *physReg.onWrite;
				physReg.onWrite = '0';
			}
		}


		scope.physicalDescription->children.emplace_back(scope.physicalDescription->size.bits(), description);
		scope.physicalDescription->size += numRegisters * registerWidth;

		if (signal.writeSignal)
			*signal.writeSignal = (BVec) pack(writeParts);

	}

	for (auto &s : scope.subScopes) {
		s.offsetInBits = scope.physicalDescription->size.bits();
		packRegisters(registerWidth, s);
		scope.physicalDescription->children.emplace_back(scope.physicalDescription->size.bits(), s.physicalDescription);
		scope.physicalDescription->size += s.physicalDescription->size;
	}
}

PackedMemoryMap::RegisteredBaseSignal *PackedMemoryMap::findSignal(Scope &scope, std::string_view name)
{
	for (auto &r : scope.registeredSignals)
		if (r.name == name)
			return &r;
	return nullptr;
}

	std::string PackedMemoryMap::listRegisteredSignals(Scope& scope, std::string prefix)
	{
		std::string ret;

		for (auto& signal : scope.registeredSignals)
			ret += prefix + '.' + signal.name + '\n';

		for (auto& subscope : scope.subScopes)
			ret += listRegisteredSignals(subscope, prefix + '.' + subscope.name);

		return ret;
	}

	PackedMemoryMap::RegisteredBaseSignal& PackedMemoryMap::findSignal(std::vector<std::string_view> path)
	{
		Scope* scope = &m_scope;

		HCL_ASSERT(path.size());
		auto it = path.begin();
		for (size_t i = 0; i < path.size() - 1; ++i, ++it)
		{
			for (Scope& subscope : scope->subScopes)
			{
				if (subscope.name == *it)
				{
					scope = &subscope;
					goto next;
				}
			}
			throw std::runtime_error{"Could not find subscope " + std::string{ *it } + ".\n " + listRegisteredSignals(*scope, "") };
		next:;
		}

		for (RegisteredBaseSignal& signal : scope->registeredSignals)
		{
			if (signal.name == *it)
				return signal;
		}
		throw std::runtime_error{"Could not find signal " + std::string{ *it } + ".\n " + listRegisteredSignals(*scope, "") };
	}

	void pinSimu(PackedMemoryMap::Scope& mmap, std::string prefix)
	{
		for (PackedMemoryMap::RegisteredBaseSignal& signal : mmap.registeredSignals)
		{
			if(signal.writeSignal)
				pinIn(*signal.writeSignal, prefix + '_' + signal.name, PinNodeParameter{ .simulationOnlyPin = true });
			else if(signal.readSignal)
				pinOut(*signal.readSignal, prefix + '_' + signal.name, PinNodeParameter{ .simulationOnlyPin = true });

			if (signal.writeSignal && signal.name == "valid")
				DesignScope::get()->getCircuit().addSimulationProcess([signalPort = *signal.writeSignal]() -> SimProcess {
					simu(signalPort) = 0;
					co_return;
				});
		}

		for (PackedMemoryMap::Scope& subscope : mmap.subScopes)
			pinSimu(subscope, prefix + '_' + subscope.name);
	}
}
