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

	class Pipeline;

	template<typename T, class En = void>
	struct PipelineInput {
		T operator()(const T &val, Pipeline &pipeline) { return val; }
	};


	class Pipeline {
		public:
			template<typename T>
			T operator()(const T &input) {
				HCL_DESIGNCHECK_HINT(!m_regSpawner->wasResolved(), "This pipeline has already been involved and resolved in retiming and can no longer be modified!");
				return PipelineInput<T>{}(input, *this);
			}

			template<typename T, typename ResetT>
			T operator()(const T &input, const ResetT &reset) {
				HCL_DESIGNCHECK_HINT(!m_regSpawner->wasResolved(), "This pipeline has already been involved and resolved in retiming and can no longer be modified!");
				return PipelineInput<T>{}(input, reset, *this);
			}

			Pipeline();

			size_t getNumPipelineStages() const;
			inline hlim::Node_RegSpawner *getRegSpawner() { return m_regSpawner.get(); }
		protected:
			hlim::NodePtr<hlim::Node_RegSpawner> m_regSpawner;
	};

    template<>
    struct PipelineInput<UInt>
    {
        UInt operator () (const UInt& signal, Pipeline &pipeline) {
			pipeline.getRegSpawner()->setClock(ClockScope::getClk().getClk());
			auto port = pipeline.getRegSpawner()->addInput(signal.getReadPort(), {});
			return SignalReadPort{hlim::NodePort{.node = pipeline.getRegSpawner(), .port = port}};
		}
        UInt operator () (const UInt& signal, const UInt& reset, Pipeline &pipeline) {
			NormalizedWidthOperands ops(signal, reset);

			pipeline.getRegSpawner()->setClock(ClockScope::getClk().getClk());
			auto port = pipeline.getRegSpawner()->addInput(ops.lhs, ops.rhs);
			return SignalReadPort{hlim::NodePort{.node = pipeline.getRegSpawner(), .port = port}};
		}
    };

    template<>
    struct PipelineInput<Bit>
    {
        Bit operator () (const Bit& signal, Pipeline &pipeline) {
			pipeline.getRegSpawner()->setClock(ClockScope::getClk().getClk());
			auto port = pipeline.getRegSpawner()->addInput(signal.getReadPort(), {});
			return SignalReadPort{hlim::NodePort{.node = pipeline.getRegSpawner(), .port = port}};
		}
        Bit operator () (const Bit& signal, const Bit& reset, Pipeline &pipeline) {
			pipeline.getRegSpawner()->setClock(ClockScope::getClk().getClk());
			auto port = pipeline.getRegSpawner()->addInput(signal.getReadPort(), reset.getReadPort());
			return SignalReadPort{hlim::NodePort{.node = pipeline.getRegSpawner(), .port = port}};
		}
    };


    struct PipelineInputTransform
    {
        PipelineInputTransform() { }

        template<typename T>
        T operator() (const T& val) { return PipelineInput<T>{}(val); }
    };

    template<typename ...T>
    struct PipelineInput<std::tuple<T...>>
    {
        std::tuple<T...> operator () (const std::tuple<T...>& val) { return boost::hana::transform(val, PipelineInputTransform{}); }
    };

	template<typename T>
	struct PipelineInput<T, std::enable_if_t<boost::spirit::traits::is_container<T>::value>>
	{
		T operator () (const T& signal, Pipeline &pipeline) {
			T ret = signal;
			for (auto& item : ret)
				item = PipelineInput<decltype(item)>{}(item, pipeline);
			return ret;
		}

		T operator () (const T& signal, const T& reset, Pipeline &pipeline)
		{
			T ret = signal;

			auto itS = begin(ret);
			auto itR = begin(reset);
			for (; itS != end(ret) && itR != end(reset); ++itR, ++itS)
				*itS = PipelineInput<decltype(*itS)>{}(*itS, *itR, pipeline);
			for (; itS != end(ret); ++itS)
				*itS = PipelineInput<decltype(*itS)>{}(*itS, pipeline);
			return ret;
		}
	};

	template<typename T>
	struct PipelineInput<T, std::enable_if_t<boost::hana::Struct<T>::value>>
	{
		T operator () (const T& signal, Pipeline &pipeline)
		{
			T ret = signal;
			boost::hana::for_each(boost::hana::accessors<std::remove_cvref_t<T>>(), [&](auto&& member) {
				auto& subSig = boost::hana::second(member)(ret);
				subSig = PipelineInput<std::remove_cvref_t<decltype(subSig)>>{}(subSig, pipeline);
			});
			return ret;
		}

		T operator () (const T& signal, const T& reset, Pipeline &pipeline)
		{
			T ret = signal;
			boost::hana::for_each(boost::hana::accessors<std::remove_cvref_t<T>>(), [&](auto member) {
				auto& subSig = boost::hana::second(member)(ret);
				const auto& subResetSig = boost::hana::second(member)(reset);
				subSig = PipelineInput<std::remove_cvref_t<decltype(subSig)>>{}(subSig, subResetSig, pipeline);
				});
			return ret;
		}
	};



	template<class T, class En = void>
	struct RegHint
	{
		T operator () (const T& val) { return val; }
	};

    struct RegHintTransform
    {
        RegHintTransform() { }

        template<typename T>
        T operator() (const T& val) { return RegHint<T>{}(val); }
    };
    
    template<typename ...T>
    struct RegHint<std::tuple<T...>>
    {
        std::tuple<T...> operator () (const std::tuple<T...>& val) { return boost::hana::transform(val, RegHintTransform{}); }
    };

	template<typename T>
	T regHint(const T& val) { return RegHint<T>{}(val); }

	Bit placeRegHint(const Bit &bit);
	UInt placeRegHint(const UInt &UInt);

    template<>
    struct RegHint<UInt>
    {
        UInt operator () (const UInt& signal) { return placeRegHint(signal); }
    };

    template<>
    struct RegHint<Bit>
    {
        Bit operator () (const Bit& signal) { return placeRegHint(signal); }
    };	

	template<typename T>
	struct RegHint<T, std::enable_if_t<boost::spirit::traits::is_container<T>::value>>
	{
		T operator () (const T& signal)
		{
			T ret = signal;
			for (auto& item : ret)
				item = regHint(item);
			return ret;
		}
	};

	template<typename T>
	struct RegHint<T, std::enable_if_t<boost::hana::Struct<T>::value>>
	{
		T operator () (const T& signal)
		{
			T ret = signal;
			boost::hana::for_each(boost::hana::accessors<std::remove_cvref_t<T>>(), [&](auto&& member) {
				auto& subSig = boost::hana::second(member)(ret);
				subSig = regHint(subSig);
			});
			return ret;
		}
	};


}
