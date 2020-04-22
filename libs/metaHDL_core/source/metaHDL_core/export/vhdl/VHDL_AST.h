#pragma once

#include "CodeFormatting.h"

#include "../../hlim/Node.h"
#include "../../hlim/NodeGroup.h"
#include "../../hlim/Circuit.h"


#include <set>
#include <map>
#include <string>
#include <list>
#include <map>

namespace mhdl {
namespace core {
namespace vhdl {
    

namespace ast {

class Namespace {
    public:
        /// Creates/retrieves name of a node (usually a signal).
        std::string getName(const hlim::BaseNode* node);
        /// Creates/retrieves name of a global entity (usually clock and reset signals).
        std::string getGlobalsName(const std::string &id);

        void setup(Namespace *parent, CodeFormatting *codeFormatting);
    protected:
        bool isNameInUse(const std::string &name) const;
        
        Namespace *m_parent = nullptr;
        CodeFormatting *m_codeFormatting = nullptr;
        std::set<std::string> m_namesInUse;
        std::map<const hlim::BaseNode*, std::string> m_nodeNames;
        std::map<std::string, std::string> m_globalsNames;
};

struct Locals
{
    std::set<hlim::Node_Signal*> signals;
    std::set<hlim::Node_Signal*> variables;
};

struct IO {
    std::map<hlim::Node_Signal*, hlim::NodeGroup*> inputs;
    std::map<hlim::Node_Signal*, hlim::NodeGroup*> outputs;
    // clock, reset, pins, ...
    std::vector<std::string> globalInputs;
    // clock, reset, pins, ...
    std::vector<std::string> globalOutputs;
};

struct Assignment {
    hlim::Node_Signal *recievingSignal;
};

struct ConditionalAssignments {
    std::vector<Assignment> truePart;
    std::vector<Assignment> falsePart;
};

struct CombinatorialBlock
{
    Namespace nspace;
    Locals locals;
    std::vector<hlim::BaseNode*> nodes;
    std::vector<Assignment> unconditionalAssignments;
    std::map<hlim::Node_Signal*, ConditionalAssignments> conditionalAssignments;
};

/*
class Procedure
{
    Namespace nspace;
    IO io;
    CombinatorialBlock code;
};
*/

struct RegisterConfig
{
    std::string clockSignal;
    std::string resetSignal;
    bool raisingEdge;
    bool synchronousReset;
    
    std::vector<hlim::Node_Register*> registerNodes;
};

class Root;

class Entity
{
    public:
        Entity(Root &root);
        void buildFrom(hlim::NodeGroup *nodeGroup);

        void print();
    protected:
        Root &m_root;        
        std::string m_name;
        Namespace m_namespace;
        IO m_io;
        Locals m_locals;
        std::vector<Entity*> m_subComponents;
        std::list<CombinatorialBlock> m_combinatorialProcesses;
        std::list<RegisterConfig> m_registerConfigs;

        Entity *m_identicalEntity = nullptr;
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
        std::list<Entity> m_components;
};


}
}
}
}
