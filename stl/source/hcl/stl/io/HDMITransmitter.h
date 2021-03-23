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

core::frontend::BVec tmdsEncode(core::frontend::Clock &pixelClock, core::frontend::Bit sendData, core::frontend::BVec data, core::frontend::BVec ctrl);

core::frontend::BVec tmdsEncodeReduceTransitions(const core::frontend::BVec& data);
core::frontend::BVec tmdsDecodeReduceTransitions(const core::frontend::BVec& data);

core::frontend::BVec tmdsEncodeBitflip(const core::frontend::Clock& clk, const core::frontend::BVec& data);
core::frontend::BVec tmdsDecodeBitflip(const core::frontend::BVec& data);

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
    core::frontend::BVec r, g, b;
};

class TmdsEncoder
{
public:
    TmdsEncoder(core::frontend::Clock& clk);

    void addColorStream(const Valid<ColorRGB>& color) { setColor(color.unpack()); }
    void addSync(const core::frontend::Bit& hsync, const core::frontend::Bit& vsync);
    void addTERC4(const Valid<core::frontend::BVec>& ctrl) { setTERC4(ctrl.unpack()); }

    // alternative interface using conditionals
    void setColor(const ColorRGB& color);
    void setSync(bool hsync, bool vsync);
    void setTERC4(core::frontend::BVec ctrl);

    SerialTMDS serialOutput() const;
    const auto& channels() const { return m_Channel; }

    SerialTMDS serialOutputInPixelClock(Bit& tick) const;

protected:
    core::frontend::Clock& m_Clk;
    std::array<core::frontend::BVec, 3> m_Channel;
};

class Transmitter
{
    public:
        
    protected:
};

}
