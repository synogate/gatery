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
            void setWidth(size_t width);

            bool isOutputPin() const;

            virtual bool hasSideEffects() const override { return true; }
            virtual std::vector<size_t> getInternalStateSizes() const override;

            void setState(sim::DefaultBitVectorState &state, const size_t *internalOffsets, const sim::DefaultBitVectorState &newState);
            virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;

            virtual std::string getTypeName() const override;
            virtual void assertValidity() const override;
            virtual std::string getInputName(size_t idx) const override;
            virtual std::string getOutputName(size_t idx) const override;

            virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

            virtual std::string attemptInferOutputName(size_t outputPort) const;
    };

}
