#pragma once

#include "NodeGroup.h"

#include <set>
#include <map>
#include <string>
#include <memory>
#include <functional>

namespace mhdl {
namespace core {
namespace hlim {

class BaseNode;
class Node_Signal;
class Node_Register;


struct NodeCategorization
{
    std::map<Node_Signal*, NodeGroup*> inputSignals;
    std::map<Node_Signal*, NodeGroup*> outputSignals;
    std::map<Node_Signal*, NodeGroup*> childInputSignals;
    std::map<Node_Signal*, NodeGroup*> childOutputSignals;
    std::set<Node_Signal*> internalSignals;
    std::set<Node_Register*> registers;
    std::set<BaseNode*> combinatorial;
    std::set<BaseNode*> unconnected;
    std::set<BaseNode*> unused;
    std::set<NodeGroup*> childGroups;
    
    void parse(NodeGroup *group, const std::function<bool(NodeGroup *child)> &includeChild);
};

}
}
}
