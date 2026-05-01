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
#include "../stream/strm.h"

namespace gtry::scl::rivet
{
	struct ExecResult
	{
		std::string_view name;
		BVec value;
	};

	struct ResultCollection
	{
		std::vector<ExecResult> result;

		void set(ExecResult&& r) { result.push_back(move(r)); /* TODO: remove old result of same name */ }
	};
}

namespace gtry::scl::rivet::mod
{
	template<typename T>
	class ResultSelect : public T
	{
	public:


	protected:

		virtual UInt selectResult(std::string_view area, const ResultCollection& results, const InstrOpcode& instr, std::function<const std::string& (const InstrDef&)> proj)
		{
			UInt ret{ "32bX" };

			// build list of unique targets used by instr set
			std::vector<std::string_view> targets;
			for (const InstrDef& def : T::instructions())
				if (const std::string& val = proj(def); !val.empty())
					targets.push_back(val);
			std::sort(targets.begin(), targets.end());
			targets.erase(std::unique(targets.begin(), targets.end()), targets.end());

			// find target address
			for (std::string_view target : targets)
			{
				auto it = std::ranges::find(results.result, target, &ExecResult::name);
				if (it == results.result.end())
					throw std::runtime_error("cpu pipeline does not provide " + std::string{ target } + " result");

				if (std::optional<Bit> select = T::findInstrCondition(instr, T::instrAction({ std::string{target} }, { std::string{} }, proj)))
				{
					*select = *select | name(std::format("{}_{}_select", area, target));

					if (target == targets.front())
						ret = (UInt)it->value & *select;
					else
						ret |= (UInt)it->value & *select;
				}
			}
			return ret;
		}


	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::ExecResult, name, value);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::ResultCollection, result);
