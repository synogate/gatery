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

namespace gtry::scl::arch::xilinx
{
	class ClockManager : public gtry::ExternalModule
	{
	public:
		ClockManager(std::string_view macroName);

		Bit locked();

		void clkIn(size_t index, const Clock& clk);
		void vcoCfg(size_t div, size_t mul);

		Clock clkOut(std::string_view name, size_t index, size_t counterDiv, bool inverted = false);

	protected:
		virtual std::string clkInPrefix(size_t index) const;

		std::map<size_t, Clock> m_clkIn;
		size_t m_vcoDiv = 0;
		size_t m_vcoMul = 0;

		std::string_view m_multiplierName = "CLKFBOUT_MULT_F";
	};

	class ClockManagerPLL : public ClockManager
	{
	public:
		ClockManagerPLL(std::string_view macroName) : ClockManager(macroName) { m_multiplierName = "CLKFBOUT_MULT"; }

	protected:
		std::string clkInPrefix(size_t index) const override { return "CLKIN"; }
	};

	// 7 Series variants

	class MMCME2_BASE : public ClockManager
	{
	public:
		MMCME2_BASE() : ClockManager("MMCME2_BASE") {}
	};

	class MMCME2_ADV : public ClockManager
	{
	public:
		MMCME2_ADV() : ClockManager("MMCME2_ADV") {}
	};

	// Ultrascale variants

	class MMCME3_BASE : public ClockManager
	{
	public:
		MMCME3_BASE() : ClockManager("MMCME3_BASE") {}
	};

	class MMCME3_ADV : public ClockManager
	{
	public:
		MMCME3_ADV() : ClockManager("MMCME3_ADV") {}
	};

	class PLLE3_BASE : public ClockManagerPLL
	{
	public:
		PLLE3_BASE() : ClockManagerPLL("PLLE3_BASE") {}
	};

	class PLLE3_ADV : public ClockManagerPLL
	{
	public:
		PLLE3_ADV() : ClockManagerPLL("PLLE3_ADV") {}
	};

	// Ultrascale+ variants

	class MMCME4_BASE : public ClockManager
	{
	public:
		MMCME4_BASE() : ClockManager("MMCME4_BASE") {}
	};

	class MMCME4_ADV : public ClockManager
	{
	public:
		MMCME4_ADV() : ClockManager("MMCME4_ADV") {}
	};

	class PLLE4_BASE : public ClockManagerPLL
	{
	public:
		PLLE4_BASE() : ClockManagerPLL("PLLE4_BASE") {}
	};

	class PLLE4_ADV : public ClockManagerPLL
	{
	public:
		PLLE4_ADV() : ClockManagerPLL("PLLE4_ADV") {}
	};

}
