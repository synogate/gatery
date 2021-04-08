/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace vpi_host
{
	enum class SignalDirection
	{
		none,
		in,
		out
	};

	struct SignalInfo
	{
		std::string name;
		uint32_t width = 0;
		SignalDirection direction = SignalDirection::none;

		template<class Archive>
		void serialize(Archive& ar, const unsigned int)
		{
			ar & name & width & direction;
		}
	};

	struct SimInfo
	{
		std::string rootModule;
		int32_t timeScale = 0;
		std::vector<SignalInfo> input;
		std::vector<SignalInfo> output;

		template<class Archive>
		void serialize(Archive& ar, const unsigned int)
		{
			ar & rootModule & timeScale & input & output;
		}
	};
}
