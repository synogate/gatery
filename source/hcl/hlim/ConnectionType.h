/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#ifndef CONNECTIONTYPE_H
#define CONNECTIONTYPE_H

#include <vector>
#include <string>
#include <memory>

namespace hcl::hlim {

#if 1
struct ConnectionType
{
    enum Interpretation {
        BOOL,
        BITVEC,
        DEPENDENCY,
    };
    
    Interpretation interpretation = BITVEC;
    
    size_t width = 0;
    
    bool operator==(const ConnectionType &rhs) const;
    bool operator!=(const ConnectionType &rhs) const { return !(*this == rhs); }
};

#else
    
class ConnectionType
{
    public:
        virtual ~ConnectionType() = default;

        virtual size_t getTotalWidth() const = 0;
    protected:
};

class BusConnectionType : public ConnectionType
{
    public:
        enum Interpretation {
            RAW,
            UNSIGNED,
            SIGNED_2COMPLEMENT,
            ONE_HOT,
            FLOAT
        };
        
        virtual size_t getTotalWidth() const override { return m_width; }
    protected:
        Interpretation m_interpretation = RAW;
        
        size_t m_width = 1;
        size_t m_fixedPoint_denominator = 1;
        bool m_float_signBit = false;
        size_t m_float_mantissaBits = 0;
        int m_float_exponentBias = 0;
};

class CompoundConnectionType : public ConnectionType
{
    public:
        struct SubConnection {
            std::string name;
            ConnectionType *type;
        };

        virtual size_t getTotalWidth() const override;
    protected:
        std::vector<SubConnection> m_subConnections;
};
#endif

}

#endif // CONNECTIONTYPE_H
