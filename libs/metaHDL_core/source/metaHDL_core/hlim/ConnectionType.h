#ifndef CONNECTIONTYPE_H
#define CONNECTIONTYPE_H

#include <vector>
#include <string>
#include <memory>

namespace mhdl::core::hlim {

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
    
    size_t width = 0;
    size_t fixedPoint_denominator = 1;
    bool float_signBit = false;
    size_t float_mantissaBits = 0;
    int float_exponentBias = 0;
    
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
