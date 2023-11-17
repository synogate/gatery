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

#include "Bit.h"
#include "BVec.h"
#include "Clock.h"
#include "DesignScope.h"
#include "Compound.h"

#include <gatery/hlim/supportNodes/Node_RegSpawner.h>
#include <gatery/hlim/supportNodes/Node_RegHint.h>
#include <gatery/hlim/NodePtr.h>

namespace gtry {

	class PipeBalanceGroup;

	template<BaseSignal T>				T pipestage(const T& signal);
	template<Signal T>					T pipestage(const T& signal);

	template<BaseSignal T>				T pipeinput(const T& signal, PipeBalanceGroup& group);
	template<Signal T>					T pipeinput(const T& signal, PipeBalanceGroup& group);
	template<BaseSignal T, typename Tr>	T pipeinput(const T& signal, const Tr& resetVal, PipeBalanceGroup& group);
	template<Signal T, typename Tr>		T pipeinput(const T& signal, const Tr& resetVal, PipeBalanceGroup& group);


	class PipeBalanceGroup {
		public:
			template<typename T>
			T operator()(const T &input) {
				return pipeinput<T>(input, *this);
			}

			template<typename T, typename ResetT>
			T operator()(const T &input, const ResetT& reset) {
				return pipeinput<T, ResetT>(input, reset, *this);
			}

			PipeBalanceGroup();

			size_t getNumPipeBalanceGroupStages() const;
			inline hlim::Node_RegSpawner *getRegSpawner() { return m_regSpawner.get(); }
			void verifyConsistentEnableScope();
		protected:
			hlim::NodePtr<hlim::Node_RegSpawner> m_regSpawner;
	};

	template<Signal... Targs>
	void pipeinputgroup(Targs&...args)
	{
		PipeBalanceGroup group;
		((args = pipeinput(args, group)), ...);
	}

	template<BaseSignal T>
	T pipeinput(const T& signal, PipeBalanceGroup& group)
	{
		group.verifyConsistentEnableScope();
		hlim::Node_RegSpawner* spawner = group.getRegSpawner();
		HCL_DESIGNCHECK_HINT(!spawner->wasResolved(), "This pipeBalanceGroup has already been involved and resolved in retiming and can no longer be modified!");
		spawner->setClock(ClockScope::getClk().getClk());

		return SignalReadPort{ 
			hlim::NodePort{
				.node = spawner,
				.port = spawner->addInput(signal.readPort(), {})
		}};
	}

	template<Signal T>
	T pipeinput(const T& signal, PipeBalanceGroup& group)
	{
		return internal::transformSignal(signal, [&](const auto& sig) {
			if constexpr (!Signal<decltype(sig)>)
				return sig;
			else
				return pipeinput(sig, group); // forward so it can have overloads
		});
	}

	template<BaseSignal T, typename Tr>
	T pipeinput(const T& signal, const Tr& resetVal, PipeBalanceGroup& group)
	{
		group.verifyConsistentEnableScope();
		hlim::Node_RegSpawner* spawner = group.getRegSpawner();
		HCL_DESIGNCHECK_HINT(!spawner->wasResolved(), "This pipeBalanceGroup has already been involved and resolved in retiming and can no longer be modified!");
		spawner->setClock(ClockScope::getClk().getClk());

		const T& reset = resetVal;
		if(signal.width() != reset.width())
		{
			NormalizedWidthOperands ops(signal, reset);
			return SignalReadPort{
				hlim::NodePort{
					.node = spawner,
					.port = spawner->addInput(ops.lhs, ops.rhs)
			} };
		}
		else
		{
			return SignalReadPort{
				hlim::NodePort{
					.node = spawner,
					.port = spawner->addInput(signal.readPort(), reset.readPort())
			} };
		}
	}

	template<Signal T, typename Tr>
	T pipeinput(const T& signal, const Tr& resetVal, PipeBalanceGroup& group)
	{
		return internal::transformSignal(signal, resetVal, [&](const BaseSignal auto& sig, auto&& resetSig) {
			return pipeinput(sig, resetSig, group); // forward so it can have overloads
		});
	}

	template<BaseSignal T>
	T pipestage(const T& signal)
	{
		SignalReadPort data = signal.readPort();

		auto* pipeStage = DesignScope::createNode<hlim::Node_RegHint>();
		pipeStage->connectInput(data);

		return T{ SignalReadPort(pipeStage, data.expansionPolicy) };
	}

	template<Signal T>
	T pipestage(const T& signal)
	{
		return internal::transformSignal(signal, [&](const BaseSignal auto& sig) {
			return pipestage(sig); // forward so it can have overloads
		});
	}
}
