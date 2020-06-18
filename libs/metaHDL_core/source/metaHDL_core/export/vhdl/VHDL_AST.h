#pragma once

#include "CodeFormatting.h"

#include "../../hlim/Clock.h"
#include "../../hlim/Node.h"
#include "../../hlim/NodeGroup.h"
#include "../../hlim/Circuit.h"
#include "../../hlim/NodeSet.h"


#include <filesystem>
#include <set>
#include <map>
#include <string>
#include <list>
#include <vector>

namespace mhdl::core::hlim {
    class Node_External;
}

namespace mhdl::core::vhdl::ast {

    
struct NodeGroupInfo
{
    std::vector<hlim::BaseNode*> nodes;
    std::vector<hlim::Node_External*> externalNodes;
    std::vector<hlim::NodeGroup*> subEntities;
    std::vector<hlim::NodeGroup*> subAreas;
    
    void buildFrom(hlim::NodeGroup *nodeGroup, bool mergeAreasReccursive);
};


class BaseBlock;

class Hlim2AstMapping
{
    public:
        void assignNodeToBlock(hlim::BaseNode *node, BaseBlock *block);
        BaseBlock *getBlock(hlim::BaseNode *node) const;
    protected:
        std::map<hlim::BaseNode*, BaseBlock*> m_node2Block;
};
    
    
class Namespace {
    public:
        /// Creates/retrieves name of a node (usually a signal).
        //std::string getName(hlim::NodePort nodePort, const std::string &desiredName);
        
        std::string allocateName(const std::string &desiredName, CodeFormatting::SignalType type);
        
        /// Creates/retrieves name of a global entity (usually clock and reset signals).
        std::string getGlobalsName(const std::string &id);

        void setup(Namespace *parent, CodeFormatting *codeFormatting);
    protected:
        bool isNameInUse(const std::string &name) const;
        
        Namespace *m_parent = nullptr;
        CodeFormatting *m_codeFormatting = nullptr;
        std::set<std::string> m_namesInUse;
        std::map<hlim::NodePort, std::string> m_nodeNames;
        std::map<std::string, std::string> m_globalsNames;
};


struct ExplicitSignal {
    hlim::NodePort producerOutput;
    std::string desiredName;

    bool drivenByExternal = false;
    bool drivingExternal = false;
    bool drivenByChild = false;
    bool drivingChild = false;
    
    bool hintedExplicit = false;
    bool syntaxNecessity = false;
    bool registerInput = false;
    bool registerOutput = false;
    bool multipleConsumers = false;
};


struct SignalDeclaration {
    std::vector<hlim::NodePort> inputSignals;
    std::vector<hlim::NodePort> outputSignals;
    std::vector<hlim::NodePort> localSignals;
    std::vector<hlim::NodePort> childInputSignals;
    std::vector<hlim::NodePort> childOutputSignals;
    
    std::map<std::string, std::string> resetNames;
    std::map<hlim::NodePort, std::string> signalNames;
    std::map<hlim::BaseClock*, std::string> clockNames;
};

class BaseBlock {
    public:
        BaseBlock(BaseBlock *parent, Hlim2AstMapping &hlim2astMapping, CodeFormatting *codeFormatting, hlim::NodeGroup *nodeGroup);
        virtual ~BaseBlock() { }
                
        inline std::map<hlim::NodePort, ExplicitSignal> &getExplicitSignals() { return m_explicitSignals; }
        inline SignalDeclaration &getSignalDeclaration() { return m_signalDeclaration; }
        inline Namespace &getNamespace() { return m_namespace; }
        inline hlim::NodeGroup *getNodeGroup() { return m_nodeGroup; }
        
        inline BaseBlock *getParent() const { return m_parent; }
        bool isChildOf(const BaseBlock *other) const;
        
        void inheritSignalsFromParent();        
    protected:
        BaseBlock *m_parent;
        Hlim2AstMapping &m_hlim2astMapping;
        Namespace m_namespace;
        SignalDeclaration m_signalDeclaration;        
        std::map<hlim::NodePort, ExplicitSignal> m_explicitSignals;
        std::set<std::string> m_resets;
        std::set<hlim::BaseClock*> m_clocks;
        hlim::NodeGroup *m_nodeGroup;
        std::string m_name;
        std::string m_comment;

        void extractExplicitSignals_checkInputOutput(hlim::BaseNode *node);
};

struct RegisterConfig
{
    hlim::BaseClock *clockSignal;
    std::string resetSignal;
    bool raisingEdge;
    bool synchronousReset;
};

class Entity;

class Process : public BaseBlock
{
    public:
        Process(BaseBlock *parent, Hlim2AstMapping &hlim2astMapping, CodeFormatting *codeFormatting, hlim::NodeGroup *nodeGroup, const std::vector<hlim::BaseNode*> &nodes);
        virtual ~Process() { }
        
        void extractExplicitSignals();
        
        void allocateLocalSignals();
        void allocateExternalIOSignals();
        void allocateIntraEntitySignals();
        void allocateChildEntitySignals();
        void allocateRegisterSignals();

        inline const std::string &getName() const { return m_name; }
        
        void write(std::fstream &file);
    protected:
        std::vector<hlim::BaseNode*> m_combinatoryNodes;
        std::map<RegisterConfig, std::vector<hlim::BaseNode*>> m_registerNodes;
        
        void formatExpression(std::ostream &stream, const hlim::NodePort &nodePort, std::set<hlim::NodePort> &dependentInputs, bool forceUnfold = false);
};

class Block : public BaseBlock
{
    public:
        void buildFrom(hlim::NodeGroup *nodeGroup);

        void propagateIOSignalsFromChildren();
        void extractExplicitSignals();
    protected:
        void propagateIOSignalsFromChild(BaseBlock *child);
        std::vector<Entity*> m_subEntities;
        std::vector<hlim::Node_External*> m_externalNodes;
        std::list<Process> m_processes;
};

class Root;

class Entity : public Block
{
    public:
        Entity(Root &root);
        void buildFrom(hlim::NodeGroup *nodeGroup);
        

        void print();
        void write(std::filesystem::path destination);
        
        
        inline Root &getRoot() { return m_root; }
    protected:
        Root &m_root;        

        Entity *m_identicalEntity = nullptr;
        std::list<Block> m_blocks;
};

class Root
{
    public:
        Root(CodeFormatting *codeFormatting);
        
        Entity &createEntity();

        inline Namespace &getNamespace() { return m_namespace; }
        inline CodeFormatting *getCodeFormatting() { return m_codeFormatting; }
        
        void print();
        
        void write(std::filesystem::path destination);
    protected:
        CodeFormatting *m_codeFormatting;
        Namespace m_namespace;
        std::list<Entity> m_entities;
};


}
