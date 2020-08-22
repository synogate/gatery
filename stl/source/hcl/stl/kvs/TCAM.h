#pragma once
#include <hcl/frontend.h>
#include "../Avalon.h"
#include "../algorithm/Stream.h"

namespace hcl::stl
{
    // returns a combinatorial bit vector of matches if write == '0'
    core::frontend::BVec constructTCAMCell(
        const core::frontend::BVec& searchKey, 
        const core::frontend::Bit& write, 
        const std::vector<core::frontend::BVec>& writeData);

    struct LutCAMSimpleStreamRequest
    {
        core::frontend::BVec searchKey;

        core::frontend::Bit update;
        core::frontend::BVec updateLutAddr;
        core::frontend::BVec updateLutData;
    };

    class LutTCAM
    {
    public:

        void setLutSize(size_t addrWidth, size_t dataWidth);
        void setSize(size_t numElements, size_t bitsPerElement);
        void setInput(const Valid<LutCAMSimpleStreamRequest>& in);
        void setMemoryType(std::function<void(AvalonMM&)> ramFactory);
        void setPerElementReduce();

        Valid<core::frontend::BVec> getResultIndex();


    protected:
        size_t getNumBits() const { return m_numElements * m_bitsPerElement; }
        size_t getNumLuts() const { return (getNumBits() - 1) / m_lutAddrWidth + 1; }
        size_t getNumLutsPerElement() const { return m_bitsPerElement / m_lutAddrWidth; }

    private:
        size_t m_numElements = 0;
        size_t m_bitsPerElement = 0;

        size_t m_lutAddrWidth = 5;
        size_t m_lutDataWidth = 20;

        std::vector<AvalonMM> m_luts;
        core::frontend::BVec m_match;
        core::frontend::Bit m_valid;
    };

}
