#pragma once

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
        
        size_t getWidth() const { return m_exitNode ? getConnType().width : 0; }
        
        inline const hlim::ConnectionType& getConnType() const { hlim::NodePort port = getReadPort(); return port.node->getOutputConnectionType(port.port); }
        inline hlim::NodePort getReadPort() const { return { .node = m_valueNode, .port = 0ull }; }
        inline const std::string& getName() const { return m_exitNode->getName(); }
        inline hlim::Node_Signal *getSignalNode() const { return m_exitNode; }
        virtual void setName(std::string name) override;
        
    protected:
        enum class InitInvalid { x };
        enum class InitPrimordial { x };
        enum class InitCopyCtor { x };
        enum class InitOperation { x };
        enum class InitUnconnected { x };
        
        ElementarySignal(InitInvalid);
        ElementarySignal(const hlim::ConnectionType& connType, InitUnconnected);
        ElementarySignal(const hlim::NodePort &port, InitOperation);
        ElementarySignal(const ElementarySignal& rhs, InitCopyCtor);
        ElementarySignal(const ElementarySignal& successor, InitPrimordial);
        
        ElementarySignal &operator=(const ElementarySignal&) = delete;
        ElementarySignal &operator=(const ElementarySignal&&) = delete;

        virtual void assign(const ElementarySignal &rhs);
        virtual hlim::ConnectionType getSignalType(size_t width) const = 0;

        void setConnectionType(const hlim::ConnectionType& connectionType);
        
        void initPrimordial(const ElementarySignal& successor);
    private:
        void init(const hlim::ConnectionType& connType);

        hlim::Node_Signal* m_exitNode = nullptr;
        hlim::Node_Signal* m_entryNode = nullptr;
        hlim::Node_Signal* m_valueNode = nullptr;
    };

}
