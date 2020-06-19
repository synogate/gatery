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

/*
struct NodeCategorization
{
    std::set<NodePort> drivenByExternal;
    std::set<NodePort> drivingExternal;
    std::set<NodePort> drivenByChild;
    std::set<NodePort> drivingChild;
    std::set<Node_Signal*> internalSignals;
    std::set<Node_Register*> registers;
    std::set<BaseNode*> combinatorial;
    std::set<BaseNode*> unconnected;
    std::set<BaseNode*> unused;
    std::set<NodeGroup*> childGroups;
    
    void parse(NodeGroup *group);
};
*/
}
}
}
