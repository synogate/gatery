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

#include "gatery/pch.h"
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

	m_scope.physicalDescription.offsetInBits = 0;
	packRegisters(registerWidth, m_scope);
}

void PackedMemoryMap::packRegisters(BitWidth registerWidth, Scope &scope)
{
	scope.physicalDescription.name = scope.name;
	if (scope.annotation != nullptr) {
		scope.physicalDescription.descShort = std::string(scope.annotation->shortDesc);
		scope.physicalDescription.descLong = std::string(scope.annotation->longDesc);
	}

	scope.physicalDescription.size = 0_b;
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

		for (auto i : utils::Range(numRegisters)) {
			scope.physicalRegisters.push_back({});
			auto &physReg = scope.physicalRegisters.back();
			physReg.description.offsetInBits = scope.physicalDescription.offsetInBits + scope.physicalDescription.size.bits();
			size_t startOffset = i * registerWidth.bits();
			BitWidth chunkSize = std::min(registerWidth, signalWidth - i * registerWidth);
			physReg.description.size = chunkSize;
			if (numRegisters > 1)
				physReg.description.name = signal.name+"_bits_"+std::to_string(startOffset) + "_to_"+std::to_string(startOffset + chunkSize.bits());
			else
				physReg.description.name = signal.name;
			if (signal.annotation != nullptr)
				physReg.description.descShort = std::string(signal.annotation->shortDesc);

			scope.physicalDescription.size += registerWidth;
			scope.physicalDescription.children.emplace_back(physReg.description);

			if (signal.readSignal) {
				physReg.readSignal = (*signal.readSignal)(startOffset, chunkSize);
				setName(*physReg.readSignal, std::string(physReg.description.name)+"_readOut");
			}
			if (signal.writeSignal) {
				physReg.writeSignal = chunkSize;
				physReg.onWrite = Bit{};

				// Hook write signal in order to default to no-change
				setName(*physReg.writeSignal, std::string(physReg.description.name)+"_writeIn");
				writeParts[i] = *physReg.writeSignal;
				*physReg.writeSignal = oldWriteSignal(startOffset, chunkSize);
				setName(*physReg.writeSignal, std::string(physReg.description.name)+"_writeOut");

				// Don't hook the onWrite in order to default to '0'
				*signal.onWrite = *physReg.onWrite;
				physReg.onWrite = '0';
			}
		}

		if (signal.writeSignal)
			*signal.writeSignal = (BVec) pack(writeParts);

	}

	for (auto &s : scope.subScopes) {
		s.physicalDescription.offsetInBits = scope.physicalDescription.offsetInBits + scope.physicalDescription.size.bits();
		packRegisters(registerWidth, s);
		scope.physicalDescription.size += s.physicalDescription.size;
		scope.physicalDescription.children.emplace_back(s.physicalDescription);
	}
}

PackedMemoryMap::RegisteredBaseSignal *PackedMemoryMap::findSignal(Scope &scope, std::string_view name)
{
	for (auto &r : scope.registeredSignals)
		if (r.name == name)
			return &r;
	return nullptr;
}


}