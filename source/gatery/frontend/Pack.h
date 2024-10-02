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
#include "Compound.h"
#include "UInt.h"
#include <utility>

#include "DesignScope.h"

#include <gatery/hlim/SignalGroup.h>
#include <gatery/hlim/coreNodes/Node_Rewire.h>

namespace gtry
{
	template<class T>
	concept PackableSignalValue =
		SignalValue<T> or
		(std::ranges::sized_range<T> and SignalValue<std::ranges::range_value_t<T>>);

	namespace internal
	{
		void for_each_base_signal(TupleSignal auto&& signal, auto&& cb);
		void for_each_base_signal(CompoundSignal auto&& signal, auto&& cb);

		void for_each_base_signal(BaseSignal auto&& signal, auto&& cb)
		{
			cb(std::forward<decltype(signal)>(signal));
		}

		void for_each_base_signal(BaseSignalLiteral auto&& value, auto&& cb)
		{
			for_each_base_signal(ValueToBaseSignal<decltype(value)>{value}, std::forward<decltype(cb)>(cb));
		}

		void for_each_base_signal(ContainerSignal auto&& signal, auto&& cb)
		{
			for(auto&& it : signal)
				for_each_base_signal(it, cb);
		}

		void for_each_base_signal(TupleSignal auto&& signal, auto&& cb)
		{
			boost::hana::for_each(
				std::forward<decltype(signal)>(signal), 
				[&](auto&& member) {
					if constexpr(Signal<decltype(member)>)
						for_each_base_signal(std::forward<decltype(member)>(member), cb);
			});
		}

		void for_each_base_signal(CompoundSignal auto&& signal, auto&& cb)
		{
			for_each_base_signal(
				structure_tie(std::forward<decltype(signal)>(signal)), 
				std::forward<decltype(cb)>(cb)
			);
		}

		template<std::ranges::sized_range BitSeq>
		requires (!SignalValue<BitSeq>)
		void for_each_base_signal(BitSeq&& signal, auto&& cb)
		{
			for(auto&& it : signal)
				for_each_base_signal(it, cb);
		}

		void for_each_base_signal_reverse(auto&& cb) {}

		void for_each_base_signal_reverse(auto&& cb, PackableSignalValue auto&& signal, PackableSignalValue auto&& ...other)
		{
			for_each_base_signal_reverse(cb, std::forward<decltype(other)>(other)...);
			for_each_base_signal(std::forward<decltype(signal)>(signal), std::forward<decltype(cb)>(cb));
		}
	}

	UInt pack(const PackableSignalValue auto& ...compound)
	{
		std::vector<SignalReadPort> portList;
		(internal::for_each_base_signal(compound, [&](const BaseSignal auto& signal) {
			portList.push_back(signal.readPort());
		}), ...);

		auto* m_node = DesignScope::createNode<hlim::Node_Rewire>(portList.size());
		for (size_t i = 0; i < portList.size(); ++i)
			m_node->connectInput(i, portList[i]);
		m_node->setConcat();
		return SignalReadPort(m_node);
	}

	// same as pack but in reverse parameter order
	UInt cat(const PackableSignalValue auto& ...compound)
	{
		std::vector<SignalReadPort> portList;
		internal::for_each_base_signal_reverse([&](const BaseSignal auto& signal) {
			portList.push_back(signal.readPort());
		}, compound...);

		auto* m_node = DesignScope::createNode<hlim::Node_Rewire>(portList.size());
		for(size_t i = 0; i < portList.size(); ++i)
			m_node->connectInput(i, portList[i]);
		m_node->setConcat();
		return SignalReadPort(m_node);
	}

	template<typename... Comp>
	void unpack(const UInt& vec, Comp&& ... compound)
	{
		auto&& readPort = vec.readPort();
		size_t bitOffset = 0;
		(internal::for_each_base_signal(compound, [&](BaseSignal auto& signal) {

			hlim::ConnectionType sigType = connType(signal.readPort());
			HCL_DESIGNCHECK_HINT(sigType.width + bitOffset <= vec.size(), "parameter width missmatch during unpack");

			auto* node = DesignScope::createNode<hlim::Node_Rewire>(1);
			node->recordStackTrace();
			node->connectInput(0, readPort);

			node->changeOutputType(sigType);
			node->setExtract(bitOffset, sigType.width);
			bitOffset += sigType.width;

			using TOut = std::remove_cvref_t<decltype(signal)>;
			signal = TOut{ SignalReadPort(node) };

		}), ...);

		HCL_DESIGNCHECK_HINT(bitOffset == vec.size(), "parameter width missmatch during unpack");
	}

	template<typename... Comp>
	void unpack(const BVec& vec, Comp&& ... compound)
	{
		unpack((UInt) vec, compound...);
	}
}
