#pragma once

#include <iterator>

namespace mhdl {
namespace utils {

template<class Elements>
class LinkedList;
    
template<class Host>
class LinkedListEntry
{
    public:
        using List = LinkedList<Host>;
        using ListEntry = LinkedListEntry<Host>;
        
        LinkedListEntry(Host &host) : m_host(host) { }
        ~LinkedListEntry();
    protected:
        List *m_list = nullptr;
        ListEntry *m_prev = nullptr;
        ListEntry *m_next = nullptr;
        Host &m_host;
        
        friend class LinkedList<Host>::iterator;
        friend class LinkedList<Host>;
};

template<class Elements>
class LinkedList
{
    public:
        using ListEntry = LinkedListEntry<Elements>;
        
        LinkedList() = default;
        LinkedList(const LinkedList &) = delete;
        void operator=(const LinkedList &) = delete;
        
        class iterator {
            public:
                using iterator_category = std::forward_iterator_tag;                

                iterator() { }
                iterator(const iterator &other) : m_current(other.m_current) { }
                iterator(ListEntry &current) : m_current(current) { }
                
                iterator &operator++() { m_current = m_current->m_next; return *this; }
                iterator operator++(int) { iterator oldIter = *this; this->operator++(); return oldIter; }
                bool operator==(const iterator &rhs) const { return m_current == rhs.m_current; }
                bool operator!=(const iterator &rhs) const { return m_current != rhs.m_current; }
                Elements &operator*() { return m_current->m_host; }
            protected:
                ListEntry m_current = nullptr;
        };
        
        iterator begin() { return iterator(m_first); }
        iterator end() { return iterator(); }
        
        unsigned size() const { return m_count; }
        bool empty() const { return m_count == 0; }
        
        void deleteAll();
        
        void insertBack(ListEntry &le);
        void remove(ListEntry &le);
        
        
        ListEntry *getFirst() { return m_first; }
        ListEntry *getLast() { return m_last; }
        
        Elements &front() { return m_first->m_host; }
        Elements &back() { return m_last->m_host; }
    protected:
        ListEntry *m_first = nullptr;
        ListEntry *m_last = nullptr;
        unsigned m_count = 0;
};

}
}
