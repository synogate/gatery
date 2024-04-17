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
#include <gatery/frontend.h>
#include <gatery/utils/StableContainers.h>
#include "AddressSpaceDescription.h"

#include <boost/flyweight.hpp>

#include <ostream>


namespace gtry::scl
{

/**
 * @addtogroup gtry_scl_memorymaps
 * @{
 */

	/**
	 * @brief Interface and no-op fallback implementation for the automatic generation of memory mapped control registers.
	 * @details To register signals, use the @ref gtry::scl::mapIn and @ref gtry::scl::mapOut freestanding functions.
	 * @see gtry_scl_memorymaps_page
	 */
	class MemoryMap
	{
		public:

			/**
			 * @brief Handle returned by @ref gtry::scl::mapIn and @ref gtry::scl::mapIn to determine when signals are written by the bus master.
			 * @details Allows to query if specific members of a compound are being written by the bus master (in the cycle in which they are written).
			 * @warning Not yet fully implemented.
			 */
			class SelectionHandle {
				public:
					static SelectionHandle NeverWritten() { SelectionHandle handle; handle.m_neverWritten = true; return handle; }
					static SelectionHandle SingleSignal(const ElementarySignal &signal, Bit onWrite) { SelectionHandle handle; handle.m_fieldsSelected[signal.readPort()] = onWrite; return handle; }

					Bit any();
					Bit get(const ElementarySignal &s) { return m_fieldsSelected.find(s.readPort())->second; }

					void joinWith(SelectionHandle &&rhs);
				protected:
					bool m_neverWritten = false;
					utils::StableMap<hlim::NodePort, Bit> m_fieldsSelected;
			};

			enum Flags
			{
				F_READ = 1,
				F_WRITE = 2,
			};

			virtual void enterScope(std::string_view name, const CompoundAnnotation *anotation = nullptr) { }
			virtual void leaveScope() { }

			virtual void readable(const ElementarySignal &value, std::string_view name, const CompoundMemberAnnotation *annotation = nullptr) { }
			virtual SelectionHandle writeable(ElementarySignal &value, std::string_view name, const CompoundMemberAnnotation *annotation = nullptr) { return SelectionHandle::NeverWritten(); }
			virtual void reserve(BitWidth width, std::string_view name) { }

			void flags(size_t f) { m_flags = f; }
			bool readEnabled() const { return (m_flags & F_READ) != 0; }
			bool writeEnabled() const { return (m_flags & F_WRITE) != 0; }

		protected:
			size_t m_flags = F_READ | F_WRITE;
	};

#if 0

	inline Bit gtry::scl::MemoryMap::add(Bit& value, RegDesc desc)
	{
		if (m_flags == (F_READ | F_WRITE))
			return rw(value, desc);
		else if (m_flags == F_WRITE)
			return wo(value, desc);
		else if (m_flags == F_READ)
			ro(value, desc);
		else
			HCL_DESIGNCHECK_HINT(false, "unsupported combination of flags");

		return '0';
	}

	inline Bit scl::MemoryMap::add(UInt& value, RegDesc desc)
	{
		if (m_flags == (F_READ | F_WRITE))
			return rw(value, desc);
		else if (m_flags == F_WRITE)
			return wo(value, desc);
		else if (m_flags == F_READ)
			ro(value, desc);
		else
			HCL_DESIGNCHECK_HINT(false, "unsupported combination of flags");

		return '0';
	}

	template<typename T>
	inline void MemoryMap::stage(Memory<T>& mem)
	{
		struct SigVis : CompoundNameVisitor
		{
			virtual void elementaryOnly(UInt& a) final
			{
				mmap->add(a, { .name = makeName() });
			}

			virtual void elementaryOnly(Bit& a) final
			{
				mmap->add(a, { .name = makeName() });
			}

			MemoryMap* mmap;
		};


		UInt cmdAddr = "32xX";
		Bit cmdTrigger = wo(cmdAddr, {
			.name = "cmd"
		});
		HCL_NAMED(cmdTrigger);
		HCL_NAMED(cmdAddr);

		auto&& port = mem[cmdAddr(0, mem.addressWidth())];

		T memContent = port.read();
		T stage = constructFrom(memContent);

		SigVis v{ this };
		VisitCompound<T>{}(stage, v);
		stage = reg(stage);

		IF(writeEnabled() & cmdTrigger & cmdAddr.msb() == '0')
			port = stage;

		if (readEnabled())
		{
			Bit readCmd = reg(cmdTrigger & cmdAddr.msb() == '1', '0');
			T readData = reg(memContent);
			HCL_NAMED(readCmd);
			HCL_NAMED(readData);

			IF(readCmd)
				stage = readData;
		}
	}

	template<typename T>
	inline void scl::MemoryMap::stage(std::vector<Memory<T>>& mems)
	{
		if (mems.empty())
			return;

		GroupScope ent{ GroupScope::GroupType::ENTITY, "MemoryMapRamStage" };

		struct SigVis : CompoundNameVisitor
		{
			virtual void operator () (UInt& a) final
			{
				mmap->add(a, { .name = makeName() });
				regCount++;
			}

			virtual void operator () (Bit& a) final
			{
				mmap->add(a, { .name = makeName() });
				regCount++;
			}

			MemoryMap* mmap;
			size_t regCount = 0;
		};

		BitWidth memTabWidth{ utils::Log2C(mems.size()) };
		BitWidth memWidth;
		for (const Memory<T>& m : mems)
			memWidth.value = std::max(memWidth.value, m.addressWidth().value);

		HCL_DESIGNCHECK_HINT(memWidth.value + memTabWidth.value + 2 <= 32,
			"The memory vector stage command register is limited to 32bit including 2 command bits, the table selection bits and the memory address bits.");

		Selection memTabSel = Selection::Slice(memWidth.value, memTabWidth.value);

		std::string desc;
		std::string triggerDescShort;
		std::string triggerDescLong;
		if (readEnabled() && !writeEnabled()) {
			desc = "Command register that controls and initiates transfer of data from one of the attached memories into the staging register(s). The transfer is initiated by writing to the command register.";
			triggerDescShort = "Must be 1";
			triggerDescLong = "This bit is reserved for the r/w mode, in which it is asserted if reading from memory.";
		}
		if (!readEnabled() && writeEnabled()) {
			desc = "Command register that controls and initiates transfer of data from the staging register(s) into one of the attached memories. The transfer is initiated by writing to the command register.";
			triggerDescShort = "Must be 0";
			triggerDescLong = "This bit is reserved for the r/w mode, in which it is asserted if reading from memory.";
		}
		if (readEnabled() && writeEnabled()) {
			desc = "Command register that controls and initiates transfer of data to and from one of the attached memories into the staging register(s). The transfer is initiated by writing to the command register.";
			triggerDescShort = "Read from memory";
			triggerDescLong = "Indicates the direction of the transfer. '0' transfers from the staging register(s) to the addressed memory, '1' transfers from addressed memory to the staging register(s).";
		}
		UInt cmdAddr = 32_b;
		Bit cmdTrigger = rw(cmdAddr, RegDesc{
			.name = "cmd",
			.desc = desc,
			.usedRanges = {
				RegDesc::BitRange{
					.offset = 0,
					.size = (size_t) memWidth.value,
					.descShort = "Intra table address",
					.descLong = "If the table has less address bits, the upper superfluous bits are ignored."
				},
				RegDesc::BitRange{
					.offset = (size_t) memWidth.value,
					.size = (size_t) (memWidth.value+memTabWidth.value),
					.descShort = "Table index"
				},
				RegDesc::BitRange{
					.offset = cmdAddr.size()-1,
					.size = 1,
					.descShort = triggerDescShort,
					.descLong = triggerDescLong
				},
			}
		});
		HCL_NAMED(cmdTrigger);
		HCL_NAMED(cmdAddr);

		T stage = constructFrom(mems.front().defaultValue());
		SigVis v;
		v.mmap = this;
		enterScope("staging");
		VisitCompound<T>{}(stage, v);
		leaveScope();
		stage = reg(stage);

		Bit readTrigger = reg(readEnabled() & cmdTrigger & cmdAddr.msb() == '1', '0');
		UInt readTabAddr = reg(cmdAddr(memTabSel));
		HCL_NAMED(readTrigger);
		HCL_NAMED(readTabAddr);

		for (size_t t = 0; t < mems.size(); ++t)
		{
			auto&& port = mems[t][cmdAddr(0, mems[t].addressWidth())];

			IF(cmdAddr(memTabSel) == t)
			{
				IF(writeEnabled() & cmdTrigger & cmdAddr.msb() == '0')
					port = stage;
			}

			T readData = reg(port.read());
			IF(readTrigger & readTabAddr == t)
				stage = readData;
		}

		cmdAddr = cat(
			ConstUInt(m_flags, 8_b),
			ConstUInt(mems.size(), 8_b),
			ConstUInt(memWidth.value, 8_b),
			ConstUInt(v.regCount, 8_b)
		);
	}
#endif

/**@}*/
}
