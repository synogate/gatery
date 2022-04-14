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
#include "gatery/pch.h"
#include "SignalGenerator.h"

#include "Bit.h"
#include "BVec.h"

namespace gtry {

SimpleSignalGeneratorContext::SimpleSignalGeneratorContext(const hlim::Node_SignalGenerator *node, sim::DefaultBitVectorState *state, const size_t *outputOffsets, std::uint64_t tick) :
		m_node(node), m_state(state), m_outputOffsets(outputOffsets), m_tick(tick)
{
}

void SimpleSignalGeneratorContext::setValue(size_t output, std::uint64_t value)
{
	size_t width = m_node->getOutputConnectionType(output).width;
	m_state->insertNonStraddling(sim::DefaultConfig::VALUE, m_outputOffsets[output], width, value);
}

void SimpleSignalGeneratorContext::setDefined(size_t output, std::uint64_t defined)
{
	size_t width = m_node->getOutputConnectionType(output).width;
	m_state->insertNonStraddling(sim::DefaultConfig::DEFINED, m_outputOffsets[output], width, defined);
}

void SimpleSignalGeneratorContext::set(size_t output, std::uint64_t value, std::uint64_t defined)
{
	size_t width = m_node->getOutputConnectionType(output).width;
	m_state->insertNonStraddling(sim::DefaultConfig::DEFINED, m_outputOffsets[output], width, defined);
	m_state->insertNonStraddling(sim::DefaultConfig::VALUE, m_outputOffsets[output], width, value);	
}
	
	
hlim::Node_SignalGenerator* internal::createSigGenNode(const Clock &refClk, std::vector<SignalDesc> &signals, const std::function<void(SimpleSignalGeneratorContext &context)> &genCallback)
{
	class SigGenNode : public hlim::Node_SignalGenerator
	{
		public:
			SigGenNode(hlim::Clock *clk, std::vector<SignalDesc> &signals, const std::function<void(SimpleSignalGeneratorContext &context)> &genCallback) :
										hlim::Node_SignalGenerator(clk), m_genCallback(genCallback) {
				
				m_outputNames.resize(signals.size());
				std::vector<hlim::ConnectionType> connectionTypes;
				connectionTypes.resize(signals.size());
				for (auto i : utils::Range(signals.size())) {
					m_outputNames[i] = std::string{ signals[i].name };
					connectionTypes[i] = signals[i].connType;
				}
				
				setOutputs(connectionTypes);
			}
			SigGenNode(const SigGenNode *cloneFrom) : hlim::Node_SignalGenerator(cloneFrom->m_clocks[0]), m_genCallback(cloneFrom->m_genCallback) {
				cloneFrom->copyBaseToClone(this);
				m_outputNames = cloneFrom->m_outputNames;
			}			
			virtual std::string getOutputName(size_t idx) const override { return m_outputNames[idx]; }
	
			virtual std::unique_ptr<BaseNode> cloneUnconnected() const override {
				std::unique_ptr<BaseNode> res(new SigGenNode(this));
				return res;
			}

		protected:
			std::vector<std::string> m_outputNames;
			std::function<void(SimpleSignalGeneratorContext &context)> m_genCallback;
			
			virtual void produceSignals(gtry::sim::DefaultBitVectorState &state, const size_t *outputOffsets, size_t clockTick) const override {
				SimpleSignalGeneratorContext context(this, &state, outputOffsets, clockTick);
				m_genCallback(context);
			}
	};	
	
	return DesignScope::createNode<SigGenNode>(refClk.getClk(), signals, genCallback);
}

}
