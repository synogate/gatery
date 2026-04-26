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
#include <gatery/frontend/Bit.h>
#include <gatery/frontend/ExternalModule.h>

namespace gtry::scl::arch::colognechip
{
	class CC_BRAM : public ExternalModule
	{
	public:
		enum Port { A, B };
	public:
		CC_BRAM(std::string_view name, BitWidth portW, BitWidth addrW);

		virtual void address(Port port, UInt address, Bit enable);
		virtual BVec read(Port port);
		virtual void write(Port port, Bit writeEnable, BVec data, BVec mask);

	protected:
		static std::string pname(Port port, std::string suffix);

		const BitWidth m_portW;
		const BitWidth m_addrW;
		BitWidth m_dataW[2] = {};
	};

	class CC_BRAM_40K : public CC_BRAM
	{
	public:
		CC_BRAM_40K();


	private:
	};
}
