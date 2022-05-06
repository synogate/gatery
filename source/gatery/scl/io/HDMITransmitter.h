/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2021 Michael Offel, Andreas Ley

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
#pragma once

#include <gatery/frontend.h>

namespace gtry::scl::hdmi {
	
struct SerialTMDSPair {
	Bit pos;
	Bit neg;
};

struct SerialTMDS {
	std::array<Bit, 3> data;
	Bit clock;
};

UInt tmdsEncodeSymbol(const UInt& data);

UInt tmdsEncode(Clock &pixelClock, Bit sendData, UInt data, UInt ctrl);

UInt tmdsEncodeReduceTransitions(const UInt& data);
UInt tmdsDecodeReduceTransitions(const UInt& data);

UInt tmdsEncodeBitflip(const Clock& clk, const UInt& data);
UInt tmdsDecodeBitflip(const UInt& data);

template<typename T>
class UnpackScope : public ConditionalScope
{
public:
	UnpackScope(const Bit& condition, T& obj) : ConditionalScope(condition), m_Obj(obj) {}
	operator T& () { return m_Obj; }

private:
	T& m_Obj;
};

template<typename T>
struct Valid : T
{
	Bit valid;

	UnpackScope<T> unpack() { return { valid, *this }; }
	UnpackScope<const T> unpack() const { return { valid, *this }; }
};

struct ColorRGB
{
	UInt r, g, b;
};

class TmdsEncoder
{
public:
	TmdsEncoder(Clock& clk);

	void addColorStream(const Valid<ColorRGB>& color) { setColor(color.unpack()); }
	void addSync(const Bit& hsync, const Bit& vsync);
	void addTERC4(const Valid<UInt>& ctrl) { setTERC4(ctrl.unpack()); }

	// alternative interface using conditionals
	void setColor(const ColorRGB& color);
	void setSync(bool hsync, bool vsync);
	void setTERC4(UInt ctrl);

	SerialTMDS serialOutput() const;
	const auto& channels() const { return m_Channel; }

	SerialTMDS serialOutputInPixelClock(Bit& tick) const;

protected:
	Clock& m_Clk;
	std::array<UInt, 3> m_Channel;
};

class Transmitter
{
	public:
		
	protected:
};

}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::hdmi::SerialTMDS, data, clock);
