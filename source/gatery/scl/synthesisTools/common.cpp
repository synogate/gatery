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

#include "gatery/pch.h"

#include "common.h"


#include <gatery/hlim/Clock.h>
#include <gatery/export/vhdl/AST.h>
#include <gatery/export/vhdl/Entity.h>

#include <fstream>

namespace gtry {

void writeClockXDC(const vhdl::AST &ast, const std::string &fullPath)
{
	std::fstream file(fullPath.c_str(), std::fstream::out);
	file.exceptions(std::fstream::failbit | std::fstream::badbit);

	const vhdl::Entity* top = ast.getRootEntity();
	for (const hlim::Clock* clk : top->getClocks())
	{
		auto&& name = top->getNamespaceScope().getName(clk);
		hlim::ClockRational freq = clk->getAbsoluteFrequency();
		double ns = double(freq.denominator() * 1'000'000'000) / freq.numerator();

		file << "create_clock -period " << std::fixed << std::setprecision(3) << ns << " [get_ports " << name << "]\n";
	}
}

void writeClockSDC(const vhdl::AST &ast, const std::string &fullPath)
{
	// Actually for now the same syntax
	writeClockXDC(ast, fullPath);
}


}
