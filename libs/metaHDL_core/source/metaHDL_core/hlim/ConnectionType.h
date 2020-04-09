#ifndef CONNECTIONTYPE_H
#define CONNECTIONTYPE_H

#include <vector>
#include <string>
#include <memory>

namespace mhdl {
namespace core {    
namespace hlim {

#if 1
struct ConnectionType
{
    enum Interpretation {
        BOOL,
        RAW,
        UNSIGNED,
        SIGNED_2COMPLEMENT,
        ONE_HOT,
        FLOAT
    };
    
    Interpretation interpretation = RAW;
    
    unsigned width = 0;
    unsigned fixedPoint_denominator = 1;
    bool float_signBit = false;
    unsigned float_mantissaBits = 0;
    int float_exponentBias = 0;
    
    bool operator==(const ConnectionType &rhs) const;
};

#else
    
class ConnectionType
{
    public:
        virtual ~ConnectionType() = default;

        virtual unsigned getTotalWidth() const = 0;
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
        
        virtual unsigned getTotalWidth() const override { return m_width; }
    protected:
        Interpretation m_interpretation = RAW;
        
        unsigned m_width = 1;
        unsigned m_fixedPoint_denominator = 1;
        bool m_float_signBit = false;
        unsigned m_float_mantissaBits = 0;
        int m_float_exponentBias = 0;
};

class CompoundConnectionType : public ConnectionType
{
    public:
        struct SubConnection {
            std::string name;
            ConnectionType *type;
        };

        virtual unsigned getTotalWidth() const override;
    protected:
        std::vector<SubConnection> m_subConnections;
};
#endif

}
}
}

#endif // CONNECTIONTYPE_H
