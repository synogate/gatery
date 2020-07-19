#pragma once

#include "SignalConnector.h"

#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/hlim/NodeIO.h>
#include <hcl/hlim/Node.h>
#include <hcl/hlim/Circuit.h>

#include <hcl/utils/Enumerate.h>
#include <hcl/utils/Preprocessor.h>
#include <hcl/utils/Traits.h>

#include <boost/format.hpp>


namespace hcl::core::frontend {

class ConditionalScope;
    
class BaseSignal {
    public:
        using isSignal = void;

        virtual ~BaseSignal() = default;
        
        virtual const char *getSignalTypeName() = 0;
        
        virtual void setName(std::string name) { }
    protected:
        //virtual void putNextSignalNodeInGroup(hlim::NodeGroup *group) = 0;
};

template<typename T, typename = std::enable_if<utils::isSignal<T>::value>::type>
void setName(T &signal, std::string name) {
    signal.setName(std::move(name));
}

    class ElementarySignal : public BaseSignal {
    public:
        using isElementarySignal = void;
        
        virtual ~ElementarySignal();
        
        size_t getWidth() const { return getConnType().width; }
        
        inline const hlim::ConnectionType& getConnType() const { hlim::NodePort port = getReadPort(); return port.node->getOutputConnectionType(port.port); }
        inline hlim::NodePort getReadPort() const { return { .node = m_node, .port = 0ull }; }
        inline const std::string& getName() const { return m_node->getName(); }
        inline hlim::Node_Signal *getSignalNode() const { return m_node; }
        virtual void setName(std::string name) override { m_node->setName(std::move(name)); }
        
        virtual void assignConditionalScopeMuxOutput(const hlim::NodePort &port, ConditionalScope *parentScope);
    protected:
        ElementarySignal();
        ElementarySignal(const ElementarySignal&) = default;
        ElementarySignal(const hlim::NodePort &port);
        ElementarySignal &operator=(const ElementarySignal&) = delete;
        ElementarySignal &operator=(const ElementarySignal&&) = delete;
        virtual void assign(const ElementarySignal &rhs);
        virtual hlim::ConnectionType getSignalType(size_t width) const = 0;

        template<typename FinalType>
        SignalConnector<FinalType> getPrimordial() {
            return SignalConnector<FinalType>(m_startNode);
        }

        void setConnectionType(const hlim::ConnectionType& connectionType);

    private:

        mutable hlim::Node_Signal *m_node = nullptr; 
        hlim::Node_Signal *m_startNode = nullptr; 
        
        ConditionalScope *m_conditionalScope = nullptr;

        

    };

/*
hlim::Node_Signal *constructBinaryOperation(hlim::Node::OutputPort *lhs, hlim::Node::OutputPort *rhs, hlim::Node *opNode) {
    HCL_ASSERT(dynamic_cast<hlim::Node_Signal*>(lhs->node) != nullptr);
    HCL_ASSERT(dynamic_cast<hlim::Node_Signal*>(rhs->node) != nullptr);
}
*/



/*


#define HCL_SUBSIGNAL(x) \
        registerSignal(#x, x)

        
 
 
 Bundles are templated wrappers around structs/containers
 
 
 

        // Bundles
class CompoundSignal : public BaseSignal
{
    public:
        void registerSignal(const std::string &name, BaseSignal &signal);
        
        template<class Container, typename = std::enable_if<isContainer<Container>::value>::type>
        void registerSignal(const std::string &name, Container &signals) {
            for (auto pair : utils::Enumerate<Container>(signals))
                registerSignal((boost::format("%s[%i]") % name % pair.first).str(), pair.second);
        }
        
        CompoundSignal &operator=(const CompoundSignal &rhs);
        
        virtual void putNextSignalNodeInGroup(hlim::NodeGroup *group) override { }        
    protected:
        std::vector<BaseSignal*> m_subSignals;
};

*/

    
}
