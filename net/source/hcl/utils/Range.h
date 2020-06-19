#pragma once

#include <iterator>

namespace hcl::utils {
    
template<typename Integral = size_t>
class Range
{
    public:
        Range(Integral beg, Integral end) : m_beg(beg), m_end(end) { }
        Range(Integral end) : m_end(end) { }
        
        class iterator {
            public:
                using iterator_category = std::forward_iterator_tag;                

                iterator(Integral val) : m_value(val) { }
                
                iterator &operator++() { ++m_value; return *this; }
                iterator operator++(int) { iterator oldIter = *this; this->operator++(); return oldIter; }
                bool operator==(const iterator &rhs) const { return m_value == rhs.m_value; }
                bool operator!=(const iterator &rhs) const { return m_value != rhs.m_value; }
                Integral operator*() { return m_value; }
            protected:
                Integral m_value;
        };

        iterator begin() { return iterator(m_beg); }
        iterator end() { return iterator(m_end); }
    protected:
        Integral m_beg = 0;
        Integral m_end = 0;
};
    
}

