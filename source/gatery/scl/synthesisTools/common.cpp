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

#include "gatery/scl_pch.h"

#include "common.h"


#include <gatery/hlim/Clock.h>
#include <gatery/export/vhdl/AST.h>
#include <gatery/export/vhdl/Entity.h>
#include <gatery/hlim/coreNodes/Node_Pin.h>

#include <fstream>
#include <sstream>

namespace gtry {

void writeClockXDC(const vhdl::AST &ast, std::ostream& out)
{
	const vhdl::Entity* top = ast.getRootEntity();
	for (const hlim::Clock* clk : top->getClocks())
	{
		auto&& name = top->getNamespaceScope().getClock(clk).name;
		hlim::ClockRational freq = clk->absoluteFrequency();
		double ns = double(freq.denominator() * 1'000'000'000) / freq.numerator();
		if (clk->getLogicDriver(false, clk).node == nullptr)
		{
				out << "create_clock -period " << std::fixed << std::setprecision(3) << ns << " [get_ports " << name << "]\n";
		}
		
	}
}

void writeClockSDC(const vhdl::AST &ast, std::ostream& out)
{
	// Actually for now the same syntax
	writeClockXDC(ast, out);
}

std::string escapeTcl(const std::string &str)
{
	std::stringstream result;
	
	for (auto c : str)
		switch (c) {
			case '\\':
				result << '\\' << '\\';
			break;
			default:
				result << c;
		}

	return result.str();
}

}
