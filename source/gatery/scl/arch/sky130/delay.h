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

#include "standardCells.h"
#include <gatery/frontend.h>

namespace gtry::scl::arch::sky130 {
	/**
	 * @brief Delay Buffer 4-stage 0.50um length inner stage gates
	 * @url https://skywater-pdk.readthedocs.io/en/main/contents/libraries/sky130_fd_sc_hd/cells/dlygate4sd3/README.html
	 */
	class dlygate4sd3 : protected ExternalModule
	{
	public:
		dlygate4sd3(Strength strength = Strength::one, picoseconds simDelay = 375ps, Library lib = Library::highDensity);
		Bit operator()(Bit in);
	private:
		picoseconds m_simDelay;
	};

	class dlygate4sd3_factory
	{
	public:
		dlygate4sd3_factory(size_t numDelayGates = 1, Library lib = Library::highDensity);
		Bit operator()(Bit in);
	private:
		size_t m_numDelayGates;
		Library m_lib;
	};

}
