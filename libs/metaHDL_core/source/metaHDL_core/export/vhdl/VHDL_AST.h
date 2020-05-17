#pragma once

#include "CodeFormatting.h"

#include "../../hlim/Node.h"
#include "../../hlim/NodeGroup.h"
#include "../../hlim/Circuit.h"


#include <filesystem>
#include <set>
#include <map>
#include <string>
#include <list>
#include <vector>

namespace mhdl::core::vhdl::ast {

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
    // clock, reset, pins, ...
    std::vector<std::string> globalInputs;
    // clock, reset, pins, ...
    std::vector<std::string> globalOutputs;
    
    std::map<hlim::NodePort, std::string> signalNames;
};

class BaseBlock {
    public:
        BaseBlock(Namespace *parent, CodeFormatting *codeFormatting, hlim::NodeGroup *nodeGroup);
        
        void extractExplicitSignals();
        
        void allocateLocalSignals();
    protected:
        Namespace m_namespace;
        SignalDeclaration m_signalDeclaration;        
        hlim::NodeGroup *m_nodeGroup;
        std::map<hlim::NodePort, ExplicitSignal> m_explicitSignals;
        
};

class Procedure : public BaseBlock
{
    public:
    protected:
        Procedure *m_identicalProcedure = nullptr;
};

struct RegisterConfig
{
    std::string clockSignal;
    std::string resetSignal;
    bool raisingEdge;
    bool synchronousReset;
    
    std::vector<hlim::Node_Register*> registerNodes;
};

class Entity;

class Process : public BaseBlock
{
    public:
        Process(Entity &parent, hlim::NodeGroup *nodeGroup);
        
        void allocateExternalIOSignals();
        void allocateIntraEntitySignals();
        void allocateChildEntitySignals();
        void allocateRegisterSignals();

        inline const std::string &getName() const { return m_name; }
        
        void write(std::fstream &file);
        
    protected:
        Entity &m_parent;
        std::string m_name;
        std::list<RegisterConfig> m_registerConfigs;
        
        bool isInterEntityInputSignal(hlim::NodePort nodePort);
        bool isInterEntityOutputSignal(hlim::NodePort nodePort);
        
        void formatExpression(std::ostream &stream, const hlim::NodePort &nodePort, std::set<hlim::NodePort> &dependentInputs, bool forceUnfold = false);
};


class Root;

class Entity
{
    public:
        Entity(Root &root);
        void buildFrom(hlim::NodeGroup *nodeGroup);

        void print();
        void write(std::filesystem::path destination);
        
        
        inline SignalDeclaration &getSignalDeclaration() { return m_signalDeclaration; }
        inline Root &getRoot() { return m_root; }
        inline Namespace &getNamespace() { return m_namespace; }
        hlim::NodeGroup *getNodeGroup() { return m_nodeGroup; }
    protected:
        Root &m_root;        
        hlim::NodeGroup *m_nodeGroup;
        std::string m_name;
        Namespace m_namespace;
        SignalDeclaration m_signalDeclaration;
        std::vector<Entity*> m_subEntities;
        std::list<Process> m_processes;

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
        std::list<Entity> m_entities;
};


}
