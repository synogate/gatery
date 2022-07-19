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
#include "flag.h"

#include <ranges>

namespace gtry::scl
{
	//template<class T> concept HasValid = requires(T & t) { valid(t); };
	//template<class T> concept HasReady = requires(T & t) { ready(t); };
	//template<class T> concept HasTransfer = requires(T & t) { transfer(t); };
	//template<class T> concept HasEop = requires(T & t) { eop(t); };
	//template<class T> concept HasSop = requires(T & t) { sop(t); };
	//
	//// default implementations for signals
	//template<Signal T> Bit ready(const T& signal) { return '1'; }
	//template<Signal T> Bit valid(const T& signal) { return '1'; }
	//template<Signal T> Bit transfer(const T& signal) { return valid(signal) & ready(signal); }
	//template<Signal T> Bit eop(const T& signal) { return '1'; }
	//
	//template<class T> requires (HasEop<T> and HasTransfer<T>)
	//Bit sop(const T& signal) { return !flag(transfer(signal), eop(signal)); }
}

namespace gtry::scl::internal
{
	static auto valid_getter = [](const auto& signal) { return valid(signal); };
	static auto ready_getter = [](const auto& signal) { return ready(signal); };
	static auto transfer_getter = [](const auto& signal) { return transfer(signal); };
	static auto eop_getter = [](const auto& signal) { return eop(signal); };
	static auto sop_getter = [](const auto& signal) { return sop(signal); };
}

namespace gtry::scl::views
{
	static auto valid = std::views::transform(gtry::scl::internal::valid_getter);
	static auto ready = std::views::transform(gtry::scl::internal::ready_getter);
	static auto transfer = std::views::transform(gtry::scl::internal::transfer_getter);
	static auto eop = std::views::transform(gtry::scl::internal::eop_getter);
	static auto sop = std::views::transform(gtry::scl::internal::sop_getter);
}
