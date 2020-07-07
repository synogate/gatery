#pragma once
#include "../Node.h"

#include <boost/format.hpp>

#include <vector>

namespace hcl::core::hlim {
    
class Node_Rewire : public Node<Node_Rewire>
{
    public:
        struct OutputRange {
            size_t subwidth;
            enum Source {
                INPUT,
                CONST_ZERO,
                CONST_ONE,
            };
            Source source;
            size_t inputIdx;
            size_t inputOffset;
        };
        
        struct RewireOperation {
            std::vector<OutputRange> ranges;
            
            bool isBitExtract(size_t& bitIndex) const;
        };
        
        Node_Rewire(size_t numInputs);
        
        void connectInput(size_t operand, const NodePort &port);
        inline void disconnectInput(size_t operand) { NodeIO::disconnectInput(operand); }        
        
        virtual void simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const override;
        
        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override { }
        virtual std::string getInputName(size_t idx) const override { return (boost::format("in_%d")%idx).str(); }
        virtual std::string getOutputName(size_t idx) const override { return "output"; }
        
        void setConcat();
        void setInterleave();
        void setExtract(size_t offset, size_t count, size_t stride = 1);        
        
        inline void setOp(RewireOperation rewireOperation) { m_rewireOperation = std::move(rewireOperation); updateConnectionType(); }
        inline const RewireOperation &getOp() const { return m_rewireOperation; }
        
        void changeOutputType(ConnectionType outputType);
    protected:
        ConnectionType m_desiredConnectionType;

        RewireOperation m_rewireOperation;
        void updateConnectionType();
        
};

}
