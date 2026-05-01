/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2026 Michael Offel, Andreas Ley

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

namespace gtry::scl::rivet
{
	enum class InstrEncoding
	{
		R, I, S, SB, U, UJ, DontCare
	};

	enum class OperandSource
	{
		Reg, Imm, Zero, IP, CSR, InvInstrSize, DontCare
	};

	struct InstrDef
	{
		// identification
		std::string_view name;
		std::string_view opcode; // 5 bit opcode. can contain dont care bits.
		std::string_view func3;

		// decoding
		InstrEncoding encoding = InstrEncoding::DontCare;

		// operation
		std::array<OperandSource, 2> sumSrc = { {OperandSource::DontCare, OperandSource::DontCare} };
		std::array<OperandSource, 2> difSrc = { {OperandSource::DontCare, OperandSource::DontCare} };

		// result
		std::string resultSource; // Alu, Shift, Mul, ...
		std::string jmpTarget; // Alu, ecall-vector, ...
	};
}

namespace gtry::scl::rivet::mod
{
	template<typename T>
	class InstrSet : public T
	{
	public:

		std::span<const InstrDef> instructions() const { return m_instructions; }

		BitWidth dataPathW() const { return 32_b; }

	protected:
		void addInstr(InstrDef definition) { m_instructions.push_back(definition); }

	private:
		std::vector<InstrDef> m_instructions;
	};
}
