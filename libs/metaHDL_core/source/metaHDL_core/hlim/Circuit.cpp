#include "Circuit.h"


namespace mhdl::core::hlim {

Circuit::Circuit()
{
    m_root.reset(new NodeGroup());
}


}
