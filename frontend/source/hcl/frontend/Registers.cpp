#include "Registers.h"


namespace hcl::core::frontend {

RegisterFactory::RegisterFactory(const RegisterConfig &registerConfig) : m_registerConfig(registerConfig)
{
}

template<>
Register<Bit>::Register(const RegisterConfig &registerConfig, Bit resetSignal)
{
    m_registerConfig = registerConfig;
    m_enableSignal = true;
    m_resetSignal = resetSignal;
}


}

