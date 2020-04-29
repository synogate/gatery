#include "Registers.h"


namespace mhdl::core::frontend {

RegisterFactory::RegisterFactory(const ClockConfig &clockConfig, const ResetConfig &resetConfig) : m_clockConfig(clockConfig), m_resetConfig(resetConfig)
{
}

}

