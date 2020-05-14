#pragma once
#include "../Node.h"

#include <string_view>

namespace mhdl::core::hlim {

    // todo: this is a parser and representation of bit values. replace by simulation data structure.
    struct ConstantData
    {
        ConstantData(std::string_view _str)
        {
            // check base prefix
            if (_str.length() >= 3 && _str.front() == '0')
            {
                switch (_str[1])
                {
                case 'x':
                case 'X':
                    base = 16;
                    break;
                case 'b':
                case 'B':
                    base = 2;
                    break;
                default:
                    MHDL_ASSERT(!"invalid literal. only hex and binary may start with 0.")
                }
                _str.remove_prefix(2);
            }

            if (base == 10)
                parseDecimal(_str);
            else
                parsePow2Base(_str);


        }

        void parseDecimal(std::string_view _str)
        {
            uint64_t acc = 0;
            for (char c : _str)
            {
                if (c == '\'')
                    continue;
                MHDL_DESIGNCHECK(std::isdigit(c));
                MHDL_DESIGNCHECK_HINT(acc != 0 || c != '0' || _str.size() == 1, "leading zeros are not allowed for decimal literals.");

                uint64_t new_acc = acc * 10 + (c - '0');
                MHDL_DESIGNCHECK_HINT(new_acc >= acc, "decimal literal overflow. use hex literal instead.");
                acc = new_acc;
            }

            do
            {
                bitVec.push_back((acc & 1) == 1);
                acc >>= 1;
            } while (acc);
        }

        void parsePow2Base(std::string_view _str)
        {
            for (char c : _str)
            {
                if (c == '\'')
                    continue;

                size_t val = ~0u;
                if (c >= '0' && c <= '9')
                    val = c - '0';
                else if (c >= 'a' && c <= 'w') // reserve 'x' for undefined
                    val = c - 'a';
                else if (c >= 'A' && c <= 'W')
                    val = c - 'A';

                MHDL_DESIGNCHECK_HINT(val < base, "invalid character " + c + " in " + std::string{ _str });

                for (size_t mask = base / 2; mask; mask /= 2)
                    bitVec.push_back((val & mask) != 0);
            }
            std::reverse(bitVec.begin(), bitVec.end());
        }

        ///@todo: Do we want DefaultBitVectorState here to also represent undefined values?
        std::vector<bool> bitVec;
        size_t base = 10;
    };
    
    class Node_Constant : public Node<Node_Constant>
    {
    public:
        Node_Constant(ConstantData value, const hlim::ConnectionType& connectionType);

        virtual void simulateReset(sim::DefaultBitVectorState &state, const size_t *outputOffsets) const override;
        
        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        const ConstantData& getValue() const { return m_Value; }
    protected:
        ConstantData m_Value;
    };
}
