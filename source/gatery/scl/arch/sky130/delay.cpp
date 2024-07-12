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
#include <gatery/scl_pch.h>
#include "delay.h"
#include <gatery/scl/io/dynamicDelay.h>

namespace gtry::scl::arch::sky130
{
	dlygate4sd3::dlygate4sd3(Strength strength, picoseconds simDelay, Library lib)
		: ExternalModule("sky130_fd_sc_" + toLibraryName(lib) + "_dlygate4sd3_" + std::to_string((size_t)strength)),
		m_simDelay(simDelay)
	{
		this->isEntity(false); this->requiresComponentDeclaration(true);
	}

	Bit dlygate4sd3::operator()(Bit in) {
		this->in("A") = in;
		Bit ret = this->out("X");
		ret.simulationOverride(simulateDelay(in, m_simDelay, "sim_delay")); //not perfect, but ok
		return ret;
	}

	dlygate4sd3_factory::dlygate4sd3_factory(Strength strength, picoseconds simDelay, Library lib)
		: m_strength(strength), m_simDelay(simDelay), m_lib(lib)
	{}

	Bit dlygate4sd3_factory::operator()(Bit in)
	{
		dlygate4sd3 instance(m_strength, m_simDelay, m_lib);
		return instance(in);
	}
};
