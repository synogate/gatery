#include "VHDL_AST.h"

#include "../../hlim/NodeCategorization.h"
#include "../../hlim/Node.h"
#include "../../hlim/coreNodes/Node_Signal.h"
#include "../../hlim/coreNodes/Node_Register.h"
#include "../../hlim/coreNodes/Node_Multiplexer.h"

#include <iostream>

namespace mhdl::core::vhdl::ast {
    
#if 0

std::string Namespace::getName(const hlim::Node* node)
{
    auto it = m_nodeNames.find(node);
    if (it != m_nodeNames.end()) return it->second;
    
    unsigned attempt = 0;
    std::string name;
    do {
        name = m_codeFormatting->getNodeName(node, attempt++);
    } while (isNameInUse(name));

    m_nodeNames[node] = name;
    m_namesInUse.insert(name);
    return name;    
}

std::string Namespace::getGlobalsName(const std::string &id)
{
    auto it = m_globalsNames.find(id);
    if (it != m_globalsNames.end()) return it->second;
    
    unsigned attempt = 0;
    std::string name;
    do {
        name = m_codeFormatting->getGlobalName(id, attempt);
    } while (isNameInUse(name));

    m_globalsNames[id] = name;
    m_namesInUse.insert(name);
    return name;    
}
        
void Namespace::setup(Namespace *parent, CodeFormatting *codeFormatting)
{
    m_parent = parent;
    m_codeFormatting = codeFormatting;
}

bool Namespace::isNameInUse(const std::string &name) const
{
    if (m_namesInUse.find(name) != m_namesInUse.end()) return true;
    if (m_parent != nullptr)
        return m_parent->isNameInUse(name);
    return false;
}


Entity::Entity(Root &root) : m_root(root)
{
    m_namespace.setup(&m_root.getNamespace(), m_root.getCodeFormatting());
}


void Entity::buildFrom(hlim::NodeGroup *nodeGroup)
{
    m_name = m_root.getNamespace().getGlobalsName(nodeGroup->getName());
    
    hlim::NodeCategorization categories;
    categories.parse(nodeGroup, [](const hlim::NodeGroup *g)->bool { return false; });

    for (auto signal : categories.inputSignals) {
        m_namespace.getName(signal.first); // Call shotgun on first attempt names
        m_io.inputs.insert(signal);
    }
    for (auto signal : categories.outputSignals) {
        m_namespace.getName(signal.first); // Call shotgun on first attempt names
        m_io.outputs.insert(signal);
    }

    // Connections from this module to submodules must be explicit signals in vhdl
    for (auto signal : categories.childInputSignals) {
        m_namespace.getName(signal.first); // Call shotgun on first attempt names
        m_locals.signals.insert(signal.first);
    }
    for (auto signal : categories.childOutputSignals) {
        m_namespace.getName(signal.first); // Call shotgun on first attempt names
        m_locals.signals.insert(signal.first);
    }

    for (hlim::NodeGroup *childGroup : categories.childGroups) {
        Entity &subComponent = m_root.createEntity();
        m_subComponents.push_back(&subComponent);
        subComponent.buildFrom(childGroup);
        
        for (auto signal : subComponent.m_io.inputs) {
            if (signal.second == nodeGroup) {
                // nothing to do
            } else if (signal.second->isChildOf(nodeGroup)) {
                // we are the crossover point, wire through local signal
                m_locals.signals.insert(signal.first);
            } else  {
                // pass up the chain
                m_io.inputs.insert(signal);
            }
        }
        for (auto signal : subComponent.m_io.outputs) {
            if (signal.second == nodeGroup) {
                // nothing to do
            } else if (signal.second->isChildOf(nodeGroup)) {
                // we are the crossover point, wire through local signal
                m_locals.signals.insert(signal.first);
            } else  {
                // pass up the chain
                m_io.outputs.insert(signal);
            }
        }
    }
    
    // register inputs and outputs must be explicit signals (since separate process)
    for (auto reg : categories.registers) {
        hlim::Node_Signal *inputSignal = nullptr;
        if (reg->getInput(0).connection != nullptr)
            inputSignal = dynamic_cast<hlim::Node_Signal*>(reg->getInput(0).connection->node);
        
        if (inputSignal != nullptr) {
            if (m_io.inputs.find(inputSignal) == m_io.inputs.end())
                m_locals.signals.insert(inputSignal);
        }
        
        ///@todo explicit reset, etc.
        
        for (auto &con : reg->getOutput(0).connections) {
            hlim::Node_Signal *outputSignal = dynamic_cast<hlim::Node_Signal*>(con->node);
            if (m_io.outputs.find(outputSignal) == m_io.outputs.end())
                m_locals.signals.insert(outputSignal);
        }
    }
    
    // Mux turn into conditional statements (if else) and the reciever of the mux must be an explicit signal/variable.
    for (auto node : categories.combinatorial) {
        hlim::Node_Multiplexer *mux = dynamic_cast<hlim::Node_Multiplexer *>(node);
        if (mux != nullptr) { 
            for (auto &con : mux->getOutput(0).connections) {
                hlim::Node_Signal *outputSignal = dynamic_cast<hlim::Node_Signal*>(con->node);
                if (m_io.outputs.find(outputSignal) == m_io.outputs.end())
                    m_locals.signals.insert(outputSignal);
            }
        }
    }
}

void Entity::print() 
{
    std::cout << "Entity: " << m_name << std::endl;
    std::cout << "   Inputs: " << std::endl;
    for (auto &s : m_io.inputs)
        std::cout << "        " << m_namespace.getName(s.first) << std::endl;
    std::cout << "   Outputs: " << std::endl;
    for (auto &s : m_io.outputs)
        std::cout << "        " << m_namespace.getName(s.first) << std::endl;
    std::cout << "   Local signals: " << std::endl;
    for (auto &s : m_locals.signals)
        std::cout << "        " << m_namespace.getName(s) << std::endl;
    std::cout << "   Sub entities: " << std::endl;
    for (auto &s : m_subComponents)
        std::cout << "        " << s->m_name << std::endl;
}

Root::Root(CodeFormatting *codeFormatting) : m_codeFormatting(codeFormatting)
{
    m_namespace.setup(nullptr, m_codeFormatting);
}

Entity &Root::createEntity()
{
    m_components.push_back(Entity(*this));
    return m_components.back();
}

void Root::print()
{
    for (auto &c : m_components)
        c.print();
}

#endif
    
}

