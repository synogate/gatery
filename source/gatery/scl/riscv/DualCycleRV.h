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
#include "riscv.h"
#include "ElfLoader.h"

namespace gtry::scl::riscv
{
	class DualCycleRV : public RV32I
	{
	public:

		DualCycleRV(BitWidth instructionAddrWidth = 32_b, BitWidth dataAddrWidth = 32_b);

		virtual Memory<UInt>& fetch(uint64_t entryPoint = 0);
		virtual void fetch(TileLinkU&& link, uint64_t entryPoint);
		virtual void fetchText(std::span<const std::byte> text, uint64_t entryPoint);
		virtual void fetchElf(std::filesystem::path elfPath, std::optional<std::filesystem::path> makeDir);

		// depricated, use fetch(link) instead
		virtual TileLinkU fetchTileLink(uint64_t entryPoint = 0);
	protected:
		virtual void generate(const UInt& instruction, const Bit& instructionValid);

		virtual void setIP(const UInt& ip);
		virtual void genRegisterFile(UInt rs1, UInt rs2, UInt rd);
		virtual UInt genInstructionPointer(uint64_t entryPoint, const Bit& instructionValid);
		virtual void genInstructionDecode(UInt instruction);

		void writeCallReturnTrace(std::string filename);

		Bit m_overrideIPValid;
		UInt m_overrideIP;

		Memory<UInt> m_rf;
		Memory<UInt> m_instructionMem;

	};

	struct EmbeddedRV32Config
	{
		std::optional<std::filesystem::path> makeDir = {}; // directory to invoke make in. no make invocation if not set.

		size_t stackOffset = 0; // base address of stack
		size_t stackByteSize = 1024; // no stack is attached if set to zero

		size_t dataBusOffset = 0;
	};

	/**
	 * @brief Utility function to instantiate RV32 based embedded state machines. The following sections are mapped:
	 *			- text section onto instruction bus (not accessable through data fetch)
	 *			- rodata section as read only memory onto data bus
	 *			- stack memory according to stackOffset and stackByteSize onto data bus
	 *			- data segment (data, bss, got, ... sections) onto data bus
	 *			- dataFetch bus is mapped at dataBusOffset
	 *			Note that address allocation is user responsibility. Make sure there is no relevant overlap between sections and dataFetch 
	 *			by using the "-Wl,--section-start=.text=0xZZZZZZ" compiler flag and stackOffset parameter.
	 */
	void embeddedRV32(TileLinkU&& dataFetch, std::filesystem::path elfPath, EmbeddedRV32Config cfg = {});
}
