#pragma once

#include <iterator>
#include <utility>
#include <functional>

namespace hcl::utils {
    
template<class Container>
class Enumerate
{
    public:
        Enumerate(Container &container) : m_container(container) { }
        
        class iterator {
            public:
                using iterator_category = std::forward_iterator_tag;                
                using DeferredIterator = typename Container::iterator;

                iterator() { }
                iterator(const iterator &other) : m_iter(other.m_iter), m_index(other.m_index) { }
                iterator(const DeferredIterator &iter) : m_iter(iter) { }
                
                iterator &operator++() { ++m_iter; ++m_index; return *this; }
                iterator operator++(int) { iterator oldIter = *this; this->operator++(); return oldIter; }
                bool operator==(const iterator &rhs) const { return m_iter == rhs.m_iter; }
                bool operator!=(const iterator &rhs) const { return m_iter != rhs.m_iter; }
                std::pair<size_t, typename DeferredIterator::value_type&> operator*() { return std::make_pair(m_index, std::ref(*m_iter)); }
            protected:
                DeferredIterator m_iter;
                size_t m_index = 0;
        };

        iterator begin() { return iterator(m_container.begin()); }
        iterator end() { return iterator(m_container.end()); }
    protected:
        Container &m_container;
};

template<class Container>
class ConstEnumerate
{
    public:
        ConstEnumerate(const Container &container) : m_container(container) { }
        
        class iterator {
            public:
                using iterator_category = std::forward_iterator_tag;                
                using DeferredIterator = typename Container::const_iterator;

                iterator() { }
                iterator(const iterator &other) : m_iter(other.m_iter), m_index(other.m_index) { }
                iterator(const DeferredIterator &iter) : m_iter(iter) { }
                
                iterator &operator++() { ++m_iter; ++m_index; return *this; }
                iterator operator++(int) { iterator oldIter = *this; this->operator++(); return oldIter; }
                bool operator==(const iterator &rhs) const { return m_iter == rhs.m_iter; }
                bool operator!=(const iterator &rhs) const { return m_iter != rhs.m_iter; }
                std::pair<size_t, const typename DeferredIterator::value_type&> operator*() { return std::make_pair(m_index, std::ref(*m_iter)); }
            protected:
                DeferredIterator m_iter;
                size_t m_index = 0;
        };

        iterator begin() { return iterator(m_container.cbegin()); }
        iterator end() { return iterator(m_container.cend()); }
    protected:
        const Container &m_container;
};

}
