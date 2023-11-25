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
#pragma once
#include <gatery/frontend.h>

namespace gtry::scl::arch::xilinx {
	/**
	* @brief This is a differential clock buffer with 2 separate outputs (the auxiliary output can output a 2x slower clock if wanted), 
	* suitable for transceiver clocking in Ultrascale+ device. 
	* See options here:
	* https://docs.xilinx.com/r/en-US/ug974-vivado-ultrascale-libraries/IBUFDS_GTE4
	* see generics here: 
	* https://xilinx.eetrend.com/files/2020-01/wen_zhang_/100046860-87900-ug576-ultrascale-gth-transceivers.pdf
	*/
	class IBUFDS_GTE4 : public gtry::ExternalModule
	{
	public:
		IBUFDS_GTE4();
		IBUFDS_GTE4& clockInput(const Clock& inClk, Bit inClkN);
		Clock clockOutGT();
		Clock clockOutAux();
		IBUFDS_GTE4& clockEnable(Bit clockEnable);
		IBUFDS_GTE4& auxDivideFreqBy2();
	protected:
		std::optional<Clock> m_inClk;
		bool m_auxDivideFreqBy2 = false;
		bool m_auxOutputSet = false;
	};
}

