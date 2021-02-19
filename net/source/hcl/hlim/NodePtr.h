#pragma once

#include <type_traits>

namespace hcl::core::hlim {

template<class NodeType>
class NodePtr
{
    public:
        NodePtr() = default;

        NodePtr(const NodePtr<NodeType> &rhs) {
            m_ptr = rhs.m_ptr;
            if (m_ptr) m_ptr->addRef();
        }
        NodePtr(NodeType *ptr) {
            m_ptr = ptr;
            if (m_ptr) m_ptr->addRef();
        }
        ~NodePtr() {
            if (m_ptr) m_ptr->removeRef();
        }

        void operator=(const NodePtr<NodeType> &rhs) {
            if (m_ptr) m_ptr->removeRef();
            m_ptr = rhs.m_ptr;
            if (m_ptr) m_ptr->addRef();
        }
        void operator=(NodeType *ptr) {
            if (m_ptr) m_ptr->removeRef();
            m_ptr = ptr;
            if (m_ptr) m_ptr->addRef();
        }


        bool operator==(const NodePtr<NodeType> &rhs) const {
            return m_ptr == rhs.m_ptr;
        }
        bool operator!=(const NodePtr<NodeType> &rhs) const {
            return m_ptr != rhs.m_ptr;
        }

        template<typename PointerType>
        bool operator==(std::enable_if_t<std::is_pointer_v<PointerType>, PointerType> rhs) const {
            return m_ptr == rhs;
        }
        template<typename PointerType>
        bool operator!=(std::enable_if_t<std::is_pointer_v<PointerType>, PointerType> rhs) const {
            return m_ptr != rhs;
        }

        operator NodeType*() const { return m_ptr; }


        NodeType &operator*() const { return *m_ptr; }
        NodeType *operator->() const { return m_ptr; }
        NodeType *get() const { return m_ptr; }

    protected:
        NodeType *m_ptr = nullptr;
};

}