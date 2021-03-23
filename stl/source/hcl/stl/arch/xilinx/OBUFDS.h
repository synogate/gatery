#pragma once

#include <hcl/hlim/supportNodes/Node_External.h>

namespace hcl::stl::arch::xilinx {

class OBUFDS : public hcl::core::hlim::Node_External
{
    public:
        OBUFDS();

        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

        virtual std::string attemptInferOutputName(size_t outputPort) const override;
    protected:
};


}