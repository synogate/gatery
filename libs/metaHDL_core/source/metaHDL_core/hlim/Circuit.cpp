#include "Circuit.h"


namespace mhdl {
namespace core {    
namespace hlim {

Circuit::Circuit()
{
    m_root.reset(new NodeGroup());
}

}
}
}
