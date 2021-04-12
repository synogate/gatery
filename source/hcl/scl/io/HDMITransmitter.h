/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once

#include <hcl/frontend.h>

namespace hcl::stl::hdmi {
    
struct SerialTMDSPair {
    core::frontend::Bit pos;
    core::frontend::Bit neg;
};

struct SerialTMDS {
    std::array<Bit, 3> data;
    Bit clock;
};

BVec tmdsEncodeSymbol(const BVec& data);

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

BOOST_HANA_ADAPT_STRUCT(hcl::stl::hdmi::SerialTMDS, data, clock);
