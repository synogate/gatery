#pragma once

#include "../Node.h"

#include <boost/variant.hpp>

namespace hcl::core::hlim {

class Node_SignalTap : public Node<Node_SignalTap>
{
    public:
        enum Level {
            LVL_ASSERT,
            LVL_WARN,
            LVL_DEBUG
        };
        enum Trigger {
            TRIG_ALWAYS,
            TRIG_FIRST_INPUT_HIGH,
            TRIG_FIRST_INPUT_LOW,
            TRIG_FIRST_CLOCK
        };
        
        struct FormattedSignal {
            unsigned inputIdx;
            unsigned format;
        };
        
        typedef boost::variant<std::string, FormattedSignal> LogMessagePart;
        
        inline void setLevel(Level level) { m_level = level; }
        inline Level getLevel() const { return m_level; }
        
        inline void setTrigger(Trigger trigger) { m_trigger = trigger; }
        inline Trigger getTrigger() const { return m_trigger; }
        
        void addInput(hlim::NodePort input);
        inline void addMessagePart(LogMessagePart part) { m_logMessage.push_back(std::move(part)); }
        
        virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const;
        
        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        virtual std::vector<size_t> getInternalStateSizes() const override;
        
        virtual bool hasSideEffects() const override { return true; }

    protected:
        Level m_level = LVL_DEBUG;
        Trigger m_trigger = TRIG_ALWAYS;
        //bool m_triggerOnlyOnce;
        
        std::vector<LogMessagePart> m_logMessage;
};

}
