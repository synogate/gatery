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
#include <gatery/utils/Traits.h>

#include <type_traits>

namespace gtry::scl::strm 
{
	namespace internal {
		struct TestMeta {};
	}

	template<class T>
	concept StreamSignal = CompoundSignal<T> and requires (T stream) { 
		{ *stream } -> std::assignable_from<typename std::remove_cvref_t<T>::Payload>;
		{ *stream.operator ->() } -> std::assignable_from<typename std::remove_cvref_t<T>::Payload>;
		requires Signal<typename std::remove_cvref_t<T>::Payload>;

		{ stream.template add(internal::TestMeta{}) } -> Signal;
		//{ stream.template remove<internal::TestMeta>() } -> Signal; // Very expensive to check
		{ std::remove_reference_t<decltype(stream)>::template has<internal::TestMeta>() } -> std::convertible_to<bool>;
		//{ stream.removeFlowControl() } -> Signal; // Very expensive to check

	//	{ stream.template transform([](Signal auto&&) { return Bit{}; }) } -> Signal;
	};

	template<StreamSignal StreamT>
	using StreamData = decltype(std::declval<StreamT>().removeFlowControl());
}
namespace gtry::scl {
	using strm::StreamSignal;
	using strm::StreamData;
}
