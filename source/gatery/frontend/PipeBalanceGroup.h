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

#include <gatery/hlim/supportNodes/Node_RegSpawner.h>
#include <gatery/hlim/NodePtr.h>

namespace gtry {

	class PipeBalanceGroup;

	template<typename T, class En = void>
	struct PipeBalanceGroupInput {
		T operator()(const T &val, PipeBalanceGroup &pipeBalanceGroup) { return val; }
	};


	class PipeBalanceGroup {
		public:
			template<typename T>
			T operator()(const T &input) {
				HCL_DESIGNCHECK_HINT(!m_regSpawner->wasResolved(), "This pipeBalanceGroup has already been involved and resolved in retiming and can no longer be modified!");
				return PipeBalanceGroupInput<T>{}(input, *this);
			}

			template<typename T, typename ResetT>
			T operator()(const T &input, const ResetT &reset) {
				HCL_DESIGNCHECK_HINT(!m_regSpawner->wasResolved(), "This pipeBalanceGroup has already been involved and resolved in retiming and can no longer be modified!");
				return PipeBalanceGroupInput<T>{}(input, reset, *this);
			}

			PipeBalanceGroup();

			size_t getNumPipeBalanceGroupStages() const;
			inline hlim::Node_RegSpawner *getRegSpawner() { return m_regSpawner.get(); }
		protected:
			hlim::NodePtr<hlim::Node_RegSpawner> m_regSpawner;
	};

    template<>
    struct PipeBalanceGroupInput<UInt>
    {
        UInt operator () (const UInt& signal, PipeBalanceGroup &pipeBalanceGroup) {
			pipeBalanceGroup.getRegSpawner()->setClock(ClockScope::getClk().getClk());
			auto port = pipeBalanceGroup.getRegSpawner()->addInput(signal.getReadPort(), {});
			return SignalReadPort{hlim::NodePort{.node = pipeBalanceGroup.getRegSpawner(), .port = port}};
		}
        UInt operator () (const UInt& signal, const UInt& reset, PipeBalanceGroup &pipeBalanceGroup) {
			NormalizedWidthOperands ops(signal, reset);

			pipeBalanceGroup.getRegSpawner()->setClock(ClockScope::getClk().getClk());
			auto port = pipeBalanceGroup.getRegSpawner()->addInput(ops.lhs, ops.rhs);
			return SignalReadPort{hlim::NodePort{.node = pipeBalanceGroup.getRegSpawner(), .port = port}};
		}
    };

    template<>
    struct PipeBalanceGroupInput<Bit>
    {
        Bit operator () (const Bit& signal, PipeBalanceGroup &pipeBalanceGroup) {
			pipeBalanceGroup.getRegSpawner()->setClock(ClockScope::getClk().getClk());
			auto port = pipeBalanceGroup.getRegSpawner()->addInput(signal.getReadPort(), {});
			return SignalReadPort{hlim::NodePort{.node = pipeBalanceGroup.getRegSpawner(), .port = port}};
		}
        Bit operator () (const Bit& signal, const Bit& reset, PipeBalanceGroup &pipeBalanceGroup) {
			pipeBalanceGroup.getRegSpawner()->setClock(ClockScope::getClk().getClk());
			auto port = pipeBalanceGroup.getRegSpawner()->addInput(signal.getReadPort(), reset.getReadPort());
			return SignalReadPort{hlim::NodePort{.node = pipeBalanceGroup.getRegSpawner(), .port = port}};
		}
    };


    struct PipeBalanceGroupInputTransform
    {
        PipeBalanceGroupInputTransform() { }

        template<typename T>
        T operator() (const T& val) { return PipeBalanceGroupInput<T>{}(val); }
    };

    template<typename ...T>
    struct PipeBalanceGroupInput<std::tuple<T...>>
    {
        std::tuple<T...> operator () (const std::tuple<T...>& val) { return boost::hana::transform(val, PipeBalanceGroupInputTransform{}); }
    };

	template<typename T>
	struct PipeBalanceGroupInput<T, std::enable_if_t<boost::spirit::traits::is_container<T>::value>>
	{
		T operator () (const T& signal, PipeBalanceGroup &pipeBalanceGroup) {
			T ret = signal;
			for (auto& item : ret)
				item = PipeBalanceGroupInput<decltype(item)>{}(item, pipeBalanceGroup);
			return ret;
		}

		T operator () (const T& signal, const T& reset, PipeBalanceGroup &pipeBalanceGroup)
		{
			T ret = signal;

			auto itS = begin(ret);
			auto itR = begin(reset);
			for (; itS != end(ret) && itR != end(reset); ++itR, ++itS)
				*itS = PipeBalanceGroupInput<decltype(*itS)>{}(*itS, *itR, pipeBalanceGroup);
			for (; itS != end(ret); ++itS)
				*itS = PipeBalanceGroupInput<decltype(*itS)>{}(*itS, pipeBalanceGroup);
			return ret;
		}
	};

	template<typename T>
	struct PipeBalanceGroupInput<T, std::enable_if_t<boost::hana::Struct<T>::value>>
	{
		T operator () (const T& signal, PipeBalanceGroup &pipeBalanceGroup)
		{
			T ret = signal;
			boost::hana::for_each(boost::hana::accessors<std::remove_cvref_t<T>>(), [&](auto&& member) {
				auto& subSig = boost::hana::second(member)(ret);
				subSig = PipeBalanceGroupInput<std::remove_cvref_t<decltype(subSig)>>{}(subSig, pipeBalanceGroup);
			});
			return ret;
		}

		T operator () (const T& signal, const T& reset, PipeBalanceGroup &pipeBalanceGroup)
		{
			T ret = signal;
			boost::hana::for_each(boost::hana::accessors<std::remove_cvref_t<T>>(), [&](auto member) {
				auto& subSig = boost::hana::second(member)(ret);
				const auto& subResetSig = boost::hana::second(member)(reset);
				subSig = PipeBalanceGroupInput<std::remove_cvref_t<decltype(subSig)>>{}(subSig, subResetSig, pipeBalanceGroup);
				});
			return ret;
		}
	};



	template<class T, class En = void>
	struct PipeStage
	{
		T operator () (const T& val) { return val; }
	};

    struct PipeStageTransform
    {
        PipeStageTransform() { }

        template<typename T>
        T operator() (const T& val) { return PipeStage<T>{}(val); }
    };
    
    template<typename ...T>
    struct PipeStage<std::tuple<T...>>
    {
        std::tuple<T...> operator () (const std::tuple<T...>& val) { return boost::hana::transform(val, PipeStageTransform{}); }
    };

	template<typename T>
	T pipeStage(const T& val) { return PipeStage<T>{}(val); }

	Bit placePipeStage(const Bit &bit);
	UInt placePipeStage(const UInt &UInt);

    template<>
    struct PipeStage<UInt>
    {
        UInt operator () (const UInt& signal) { return placePipeStage(signal); }
    };

    template<>
    struct PipeStage<Bit>
    {
        Bit operator () (const Bit& signal) { return placePipeStage(signal); }
    };	

	template<typename T>
	struct PipeStage<T, std::enable_if_t<boost::spirit::traits::is_container<T>::value>>
	{
		T operator () (const T& signal)
		{
			T ret = signal;
			for (auto& item : ret)
				item = pipeStage(item);
			return ret;
		}
	};

	template<typename T>
	struct PipeStage<T, std::enable_if_t<boost::hana::Struct<T>::value>>
	{
		T operator () (const T& signal)
		{
			T ret = signal;
			boost::hana::for_each(boost::hana::accessors<std::remove_cvref_t<T>>(), [&](auto&& member) {
				auto& subSig = boost::hana::second(member)(ret);
				subSig = pipeStage(subSig);
			});
			return ret;
		}
	};


}
