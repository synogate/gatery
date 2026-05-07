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
#include <gatery/frontend/ExternalModule.h>
#include <gatery/frontend/Overrides.h>

namespace gtry::scl::arch::colognechip
{
	class CC_ADDF : public ExternalModule
	{
	public:
		CC_ADDF() : ExternalModule("CC_ADDF") {
			requiresComponentDeclaration(true);
			isEntity(false);

			m_CO = out("CO");
			m_S = out("S");

			m_S = simOverride(m_S, A() ^ B() ^ CI());
			m_CO = simOverride(m_CO, A() & B() | A() & CI() | B() & CI());

			A() = '0';
			B() = '0';
			CI() = '0';
		}

		Bit& A() { return in("A"); }
		Bit& B() { return in("B"); }
		Bit& CI() { return in("CI"); }

		const Bit& CO() { return m_CO; }
		const Bit& S() { return m_S; }

	private:
		Bit m_S, m_CO;
	};
}

