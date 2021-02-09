#pragma once
#include "../Node.h"

namespace hcl::core::hlim 
{
    class Node_Shift : public Node<Node_Shift>
    {
    public:
        enum class dir {
            left, right
        };

        enum class fill {
            zero,       // fill with '0'
            one,        // fill with '1'
            last,       // arithmetic fill
            rotate      // fill with shift out bits
        };

        Node_Shift(dir _direction, fill _fill);

        void connectOperand(const NodePort& port);
        void connectAmount(const NodePort& port);

        virtual void simulateEvaluate(sim::SimulatorCallbacks& simCallbacks, sim::DefaultBitVectorState& state, const size_t* internalOffsets, const size_t* inputOffsets, const size_t* outputOffsets) const override;

        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        dir getDirection() const { return m_direction; }
        fill getFillMode() const { return m_fill; }

        virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;
    protected:
        const dir m_direction;
        const fill m_fill;
    };
}
