#pragma once
#include "../Node.h"

#include <string_view>

namespace hcl::core::hlim {

    class Node_Constant : public Node<Node_Constant>
    {
    public:
        Node_Constant(sim::DefaultBitVectorState value, hlim::ConnectionType::Interpretation connectionType);

        virtual void simulateReset(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *outputOffsets) const override;
        
        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        const sim::DefaultBitVectorState& getValue() const { return m_Value; }

        virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;
    protected:
        sim::DefaultBitVectorState m_Value;
    };
}
