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
#include "../TxRxStream.h"

namespace gtry::scl::eth
{
	struct RMII
	{
		Bit tx_en;
		BVec txd = 2_b;
		Reverse<Bit> crs_dv;
		Reverse<BVec> rxd = 2_b;
	};

	Reverse<TxRxStream> phy(RMII& in);

	RvStream<BVec> preambleFilter(RvStream<BVec> in);
	RvStream<BVec> preambleInsert(RvStream<BVec> in);

}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::eth::RMII, tx_en, txd, crs_dv, rxd);
