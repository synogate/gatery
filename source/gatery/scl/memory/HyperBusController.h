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
#include <gatery/scl/tilelink/tilelink.h>

namespace gtry::scl
{
	struct HyperBusLink
	{
		TileLinkUB data;
		TileLinkUL ctrl;
	};

	struct HyperBusPhy
	{
		BVec cs;
		Bit cken;
		Bit reset;

		Bidir<Bit> rwds;
		Bidir<BVec> dq = { 8_b, 8_b };

		std::vector<std::string> pinCs = { "_CS" };
		std::string_view pinClk = "_CK";
		std::string_view pinClkN;
		std::string_view pinRwds = "_RWDS";
		std::string_view pinDq = "_DQ";
	};

	void gateMateHyperBusPhy(HyperBusPhy& phy);

	class HyperBusController
	{
	public:


		
		virtual HyperBusLink generate(HyperBusPhy& phy);
	private:




	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::HyperBusPhy, cs, cken, reset, rwds, dq, pinCs, pinClk, pinClkN, pinRwds, pinDq);
