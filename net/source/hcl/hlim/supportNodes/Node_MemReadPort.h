#pragma once

#include "../Node.h"


namespace hcl::core::hlim {

class Node_Memory;

class Node_MemReadPort : public Node<Node_MemReadPort>
{
    public:
        enum class Inputs {
            memory,
            enable,
            address,
            count
        };

        enum class Outputs {
            data,
            count
        };

        Node_MemReadPort(std::size_t bitWidth);

        void connectMemory(Node_Memory *memory);
        void disconnectMemory();

        Node_Memory *getMemory();
        NodePort getDataPort();
        void connectEnable(const NodePort &output);
        void connectAddress(const NodePort &output);

        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        virtual std::vector<size_t> getInternalStateSizes() const override;        

        inline size_t getBitWidth() const { return m_bitWidth; }
    protected:
        friend class Node_Memory;
        std::size_t m_bitWidth;

};

}