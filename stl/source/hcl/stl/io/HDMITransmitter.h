#pragma once

#include <hcl/frontend.h>

namespace hcl::stl::hdmi {
    
struct SerialTMDSPair {
    core::frontend::Bit pos;
    core::frontend::Bit neg;
};

struct SerialTMDS {
    SerialTMDSPair data[3];
    SerialTMDSPair clock;
};

core::frontend::BitVector tmdsEncode(core::hlim::BaseClock *pixelClock, core::frontend::Bit sendData, core::frontend::UnsignedInteger data, core::frontend::BitVector ctrl);

core::frontend::BitVector tmdsEncodeReduceTransitions(const core::frontend::BitVector& data);
core::frontend::BitVector tmdsDecodeReduceTransitions(const core::frontend::BitVector& data);

core::frontend::BitVector tmdsEncodeBitflip(const core::frontend::RegisterFactory& clk, const core::frontend::BitVector& data);
core::frontend::BitVector tmdsDecodeBitflip(const core::frontend::BitVector& data);

template<typename T>
class UnpackScope : public core::frontend::ConditionalScope
{
public:
    UnpackScope(const core::frontend::Bit& condition, T& obj) : ConditionalScope(condition), m_Obj(obj) {}
    operator T& () { return m_Obj; }

private:
    T& m_Obj;
};

template<typename T>
struct Valid : T
{
    core::frontend::Bit valid;

    UnpackScope<T> unpack() { return { valid, *this }; }
    UnpackScope<const T> unpack() const { return { valid, *this }; }
};

struct ColorRGB
{
    core::frontend::BitVector r, g, b;
};

class TmdsEncoder
{
public:
    TmdsEncoder(core::frontend::RegisterFactory& clk);

    void addColorStream(const Valid<ColorRGB>& color) { setColor(color.unpack()); }
    void addSync(const core::frontend::Bit& hsync, const core::frontend::Bit& vsync);
    void addTERC4(const Valid<core::frontend::BitVector>& ctrl) { setTERC4(ctrl.unpack()); }

    // alternative interface using conditionals
    void setColor(const ColorRGB& color);
    void setSync(bool hsync, bool vsync);
    void setTERC4(const core::frontend::BitVector& ctrl);

    SerialTMDS serialOutput() const;
    const auto& channels() const { return m_Channel; }

protected:
    core::frontend::RegisterFactory& m_Clk;
    std::array<core::frontend::BitVector, 3> m_Channel;
};

class Transmitter
{
    public:
        
    protected:
};

}
