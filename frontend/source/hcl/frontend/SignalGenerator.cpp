#include "SignalGenerator.h"

#include "Bit.h"
#include "BitVector.h"

namespace hcl::core::frontend {

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
            
            virtual std::string getOutputName(size_t idx) const override { return m_outputNames[idx]; }
        protected:
            std::vector<std::string> m_outputNames;
            std::function<void(SimpleSignalGeneratorContext &context)> m_genCallback;
            
            virtual void produceSignals(hcl::core::sim::DefaultBitVectorState &state, const size_t *outputOffsets, size_t clockTick) const override {
                SimpleSignalGeneratorContext context(this, &state, outputOffsets, clockTick);
                m_genCallback(context);
            }
    };    
    
    return DesignScope::createNode<SigGenNode>(refClk.getClk(), signals, genCallback);
}

}
