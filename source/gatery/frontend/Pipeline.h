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
#include "BitVector.h"
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
				return PipelineInput<T>{}(input, *this);
			}

			template<typename T, typename ResetT>
			T operator()(const T &input, const ResetT &reset) {
				return PipelineInput<T>{}(input, reset, *this);
			}


			// actually needs to be computed
			///size_t getNumPipelineStages() const { return m_regSpawner->getNumStagesSpawned(); }

			Pipeline();

			inline hlim::Node_RegSpawner *getRegSpawner() { return m_regSpawner.get(); }
		protected:
			hlim::NodePtr<hlim::Node_RegSpawner> m_regSpawner;
	};

    template<>
    struct PipelineInput<BVec>
    {
        BVec operator () (const BVec& signal, Pipeline &pipeline) {
			pipeline.getRegSpawner()->setClock(ClockScope::getClk().getClk());
			auto port = pipeline.getRegSpawner()->addInput(signal.getReadPort(), {});
			return SignalReadPort{hlim::NodePort{.node = pipeline.getRegSpawner(), .port = port}};
		}
        BVec operator () (const BVec& signal, const BVec& reset, Pipeline &pipeline) {
			pipeline.getRegSpawner()->setClock(ClockScope::getClk().getClk());
			auto port = pipeline.getRegSpawner()->addInput(signal.getReadPort(), reset.getReadPort());
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

	template<typename T, typename Tr>
	T regHint(const T& val, const Tr& resetVal) { return RegHint<T>{}(val, resetVal); }

	Bit placeRegHint(const Bit &bit);
	BVec placeRegHint(const BVec &bvec);

    template<>
    struct RegHint<BVec>
    {
        BVec operator () (const BVec& signal) { return placeRegHint(signal); }
    };

    template<>
    struct RegHint<Bit>
    {
        Bit operator () (const Bit& signal) { return placeRegHint(signal); }
    };	

}
