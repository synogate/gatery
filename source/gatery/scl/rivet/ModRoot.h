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
	/**
	 * @brief Used as dummy payload for scl::Streams.
	 */
	struct Nothing {};
}

namespace gtry::scl::rivet::mod
{
	/**
	 * @brief Base class for cpu type lists. 
				Used to terminate any call that is optional and remove ready on loop back stream.
	 */
	class Root
	{
	public:


	protected:

		template<typename T>
		auto attach(T&& func)
		{
			return strm::transform<strm::InsertPos::attach>(std::forward<T>(func));
		}

		auto pipelineStage(std::string_view name)
		{
			return [name](StreamSignal auto&& in) {
				return move(in) |
					gtry::name(std::string{ name }) |
					strm::regDownstreamBlocking({ .allowRetimingBackward = true });
				};
		}
		
		template<StreamSignal T>
		void resultLoopBack(T&& in)
		{
			move(in) |
				strm::removeReady('1');
		}
	};
}
