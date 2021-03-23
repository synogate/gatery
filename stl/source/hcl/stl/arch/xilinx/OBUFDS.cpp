#include "OBUFDS.h"

namespace hcl::stl::arch::xilinx {

OBUFDS::OBUFDS()
{
    m_libraryName = "UNISIM";
    m_name = "OBUFDS";
    m_genericParameters["IOSTANDARD"] = "DEFAULT";
    m_genericParameters["SLEW"] = "SLOW";
    m_clockNames = {};
    m_resetNames = {};

    resizeInputs(1);
    resizeOutputs(2);
    setOutputConnectionType(0, {.interpretation = core::hlim::ConnectionType::BOOL, .width=1});
    setOutputConnectionType(1, {.interpretation = core::hlim::ConnectionType::BOOL, .width=1});
}

std::string OBUFDS::getTypeName() const
{
    return "OBUFDS";
}

void OBUFDS::assertValidity() const
{
}

std::string OBUFDS::getInputName(size_t idx) const
{
    return "I";
}

std::string OBUFDS::getOutputName(size_t idx) const
{
    if (idx == 0)
        return "O";
    else
        return "OB";
}

std::unique_ptr<core::hlim::BaseNode> OBUFDS::cloneUnconnected() const
{
    OBUFDS *ptr;
    std::unique_ptr<BaseNode> res(ptr = new OBUFDS());
    copyBaseToClone(res.get());

    ptr->m_libraryName = m_libraryName;
    ptr->m_name = m_name;
    ptr->m_genericParameters = m_genericParameters;

    return res;
}

std::string OBUFDS::attemptInferOutputName(size_t outputPort) const
{
    auto driver = getDriver(0);
    if (driver.node == nullptr)
        return "";
    if (driver.node->getName().empty())
        return "";

    if (outputPort == 0)
        return driver.node->getName() + "_pos";
    else
        return driver.node->getName() + "_neg";
}



}
