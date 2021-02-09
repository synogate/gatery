#pragma once

#include "../Node.h"

namespace hcl::core::hlim {
    
    class Node_Pin : public Node<Node_Pin>
    {
        public:
            Node_Pin();
        
            inline void connect(const NodePort &port) { NodeIO::connectInput(0, port); }
            inline void disconnect() { NodeIO::disconnectInput(0); }

            void setBool();
            void setWidth(unsigned width);

            virtual bool hasSideEffects() const override { return true; }

            virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;
        
            virtual std::string getTypeName() const override;
            virtual void assertValidity() const override;
            virtual std::string getInputName(size_t idx) const override;
            virtual std::string getOutputName(size_t idx) const override;

            virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;
    };

}
