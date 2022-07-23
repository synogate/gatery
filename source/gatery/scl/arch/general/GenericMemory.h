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
#pragma once

#include <gatery/frontend/tech/TechnologyCapabilities.h>
#include <gatery/frontend/tech/TechnologyMappingPattern.h>
#include <gatery/utils/BitFlags.h>

#include <gatery/hlim/supportNodes/Node_External.h>

#include <string>
#include <vector>

namespace gtry::scl::arch {

class FPGADevice;


class EmbeddedMemory {
	public:
		virtual ~EmbeddedMemory() = default;

		struct Desc {
			std::string memoryName;

			size_t size;

			MemoryCapabilities::SizeCategory sizeCategory;

			bool inputRegs;
			size_t outputRegs;

			size_t addressBits;

			bool supportsDualClock;
			bool supportsPowerOnInitialization;

			// More in git history!
		};

		virtual bool apply(hlim::NodeGroup *nodeGroup) const = 0;
		inline const Desc &getDesc() const { return m_desc; }

		virtual size_t getPriority() const { return (size_t)m_desc.sizeCategory * 1000; }

		virtual MemoryCapabilities::Choice select(hlim::NodeGroup *group, const MemoryCapabilities::Request &request) const;
	protected:
		Desc m_desc;
};


class GenericMemoryCapabilities : public MemoryCapabilities
{
	public:
		GenericMemoryCapabilities(const FPGADevice &targetDevice);
		virtual ~GenericMemoryCapabilities();

		virtual Choice select(hlim::NodeGroup *group, const Request &request) const override;

		static const char *getName() { return "mem"; }
	protected:
		const FPGADevice &m_targetDevice;
};

class EmbeddedMemoryList
{
	public:
		EmbeddedMemoryList() { }
		virtual ~EmbeddedMemoryList() = default;

		virtual void add(std::unique_ptr<EmbeddedMemory> mem);
		virtual const EmbeddedMemory *selectMemFor(hlim::NodeGroup *group, GenericMemoryCapabilities::Request request) const;

		inline const std::vector<std::unique_ptr<EmbeddedMemory>> &getList() const { return m_embeddedMemories; }
	protected:
		std::vector<std::unique_ptr<EmbeddedMemory>> m_embeddedMemories;
};

class EmbeddedMemoryPattern : public TechnologyMappingPattern
{
	public:
		EmbeddedMemoryPattern(const FPGADevice &targetDevice) : m_targetDevice(targetDevice) { }
		virtual ~EmbeddedMemoryPattern() = default;

		virtual bool scopedAttemptApply(hlim::NodeGroup *nodeGroup) const override;
	protected:
		const FPGADevice &m_targetDevice;
};


}