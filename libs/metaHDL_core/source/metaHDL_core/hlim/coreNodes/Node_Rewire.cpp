#include "Node_Rewire.h"

namespace mhdl {
namespace core {    
namespace hlim {
    
    
bool Node_Rewire::RewireOperation::isBitExtract(unsigned &bitIndex) const
{
    if (ranges.size() == 1) {
        if (ranges[0].subwidth == 1 &&
            ranges[0].source == OutputRange::INPUT &&
            ranges[0].inputIdx == 0) {
            
            bitIndex = ranges[0].inputOffset;
            return true;
        }
    }
    return false;
}

Node_Rewire::Node_Rewire(NodeGroup *group, unsigned numInputs) : Node(group, numInputs, 1)
{
    
}

}
}
}
