#pragma once

#include "BaseGrouping.h"

#include <vector>
#include <memory>

namespace hcl::core::vhdl {
    
class Entity;
class Process;
class Block;

struct NodeGroupInfo
{
    std::vector<hlim::BaseNode*> nodes;
    std::vector<hlim::Node_External*> externalNodes;
    std::vector<hlim::NodeGroup*> subEntities;
    std::vector<hlim::NodeGroup*> subAreas;
    std::vector<hlim::NodeGroup*> SFUs;
    
    void buildFrom(hlim::NodeGroup *nodeGroup, bool mergeAreasReccursive);
};
    
    
struct ConcurrentStatement
{
    enum Type {
        TYPE_ENTITY_INSTANTIATION,
        TYPE_EXT_NODE_INSTANTIATION,
        TYPE_BLOCK,
        TYPE_PROCESS,
    };
    Type type;
    union {
        Entity *entity;
        hlim::Node_External *externalNode;
        Block *block;
        Process *process;
    } ref;
    size_t sortIdx = 0;
    
    inline bool operator<(const ConcurrentStatement &rhs) { return sortIdx < rhs.sortIdx; }
};



struct ShiftRegStorage
{
    NodeInternalStorageSignal ref;
    size_t delay;
    hlim::ConnectionType type;
};

/**
 * @todo write docs
 */
class BasicBlock : public BaseGrouping
{
    public:
        BasicBlock(AST &ast, BasicBlock *parent, NamespaceScope *parentNamespace);
        virtual ~BasicBlock();

        virtual void extractSignals() override;
        virtual void allocateNames() override;
    protected:
        void collectInstantiations(hlim::NodeGroup *nodeGroup, bool reccursive);        
        void processifyNodes(const std::string &desiredProcessName, hlim::NodeGroup *nodeGroup, bool reccursive);
        void routeChildIOUpwards(BaseGrouping *child);
        virtual void writeStatementsVHDL(std::ostream &stream, unsigned indent);
        
        
        std::vector<ShiftRegStorage> m_shiftRegStorage;
        std::vector<std::unique_ptr<Process>> m_processes;
        std::vector<Entity*> m_entities;
        std::vector<hlim::Node_External*> m_externalNodes;
        
        std::vector<ConcurrentStatement> m_statements;
        
        void handleEntityInstantiation(hlim::NodeGroup *nodeGroup);
        void handleExternalNodeInstantiaton(hlim::Node_External *externalNode);
        void handleSFUInstantiaton(hlim::NodeGroup *sfu);
};


}
