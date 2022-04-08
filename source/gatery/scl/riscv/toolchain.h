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

#include <vector>
#include <string>

namespace gtry::scl {

class RiscVToolchain {
	public:
		enum class Compiler {
			GCC
		};

		enum class Arch {
			RV32I
		};

		enum class ABI {
			ILP32
		};

		RiscVToolchain &setCompiler(Compiler compiler) { m_compiler = compiler; return *this; }
		RiscVToolchain &setArch(Arch arch) { m_arch = arch; return *this; }
		RiscVToolchain &setABI(ABI abi) { m_abi = abi; return *this; }

		RiscVToolchain &setInstructionRomAddr(std::uint32_t addr) { m_instructionRomAddr = addr; return *this; }
		RiscVToolchain &setDataRamAddr(std::uint32_t addr) { m_dataRamAddr = addr; return *this; }
		RiscVToolchain &setStackSize(std::uint32_t size) { m_stackSize = size; return *this; }

		void addCode(std::string code) { m_codeModules.push_back(std::move(code)); }
		void addDefaultBootLoader();

		void build();
		std::string getDisassembly();

		const std::vector<unsigned char> &getInstructionRom() const { return m_instructionRom; }
		const std::vector<unsigned char> &getDataRom() const { return m_dataRom; }
	protected:
		Compiler m_compiler = Compiler::GCC;
		Arch m_arch = Arch::RV32I;
		ABI m_abi = ABI::ILP32;

		std::uint32_t m_instructionRomAddr = 0x00000000;
		std::uint32_t m_dataRamAddr = 0x80000000;
		std::uint32_t m_stackSize = 0x100;

		std::string m_compilerCmd = "riscv64-linux-gnu-gcc";
		std::string m_linkerCmd = "riscv64-linux-gnu-gcc";
		std::string m_objDumpCmd = "riscv64-linux-gnu-objdump";
		std::string m_objCopyCmd = "riscv64-linux-gnu-objcopy";

		std::vector<std::string> m_codeModules;
		std::vector<unsigned char> m_instructionRom;
		std::vector<unsigned char> m_dataRom;
};


}