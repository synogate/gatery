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

namespace gtry::hlim {

class Node_Arithmetic;
class Node_Signal2Clk;
class Node_Signal2Rst;
class Node_Clk2Signal;
class Node_ClkRst2Signal;
class Node_Compare;
class Node_Constant;
class Node_External;
class Node_Logic;
class Node_Multiplexer;
class Node_Pin;
class Node_PriorityConditional;
class Node_Register;
class Node_Rewire;
class Node_Shift;
class Node_Signal;
class Node_SignalGenerator;
class Node_SignalTap;

class Node_Memory;
class Node_MemPort;

class Node_Default;
class Node_ExportOverride;
class Node_Attributes;
class Node_PathAttributes;

class Node_RegSpawner;
class Node_RegHint;
class Node_NegativeRegister;

class Node_CDC;
class Node_MultiDriver;
class Node_RetimingBlocker;
	
class NodeVisitor
{
	public:
		virtual void operator()(Node_Arithmetic &node) = 0;
		virtual void operator()(Node_Signal2Clk &node) = 0;
		virtual void operator()(Node_Signal2Rst &node) = 0;
		virtual void operator()(Node_Clk2Signal &node) = 0;
		virtual void operator()(Node_ClkRst2Signal &node) = 0;
		virtual void operator()(Node_Compare &node) = 0;
		virtual void operator()(Node_Constant &node) = 0;
		virtual void operator()(Node_External &node) = 0;
		virtual void operator()(Node_Logic &node) = 0;
		virtual void operator()(Node_Multiplexer &node) = 0;
		virtual void operator()(Node_Pin &node) = 0;
		virtual void operator()(Node_PriorityConditional &node) = 0;
		virtual void operator()(Node_Register &node) = 0;
		virtual void operator()(Node_Rewire &node) = 0;
		virtual void operator()(Node_Signal &node) = 0;
		virtual void operator()(Node_Shift &node) = 0;
		virtual void operator()(Node_SignalGenerator &node) = 0;
		virtual void operator()(Node_SignalTap &node) = 0;
		virtual void operator()(Node_Memory &node) = 0;
		virtual void operator()(Node_MemPort &node) = 0;
		virtual void operator()(Node_Default &node) = 0;
		virtual void operator()(Node_ExportOverride &node) = 0;
		virtual void operator()(Node_Attributes &node) = 0;
		virtual void operator()(Node_PathAttributes &node) = 0;
		virtual void operator()(Node_RegSpawner &node) = 0;
		virtual void operator()(Node_RegHint &node) = 0;
		virtual void operator()(Node_NegativeRegister &node) = 0;
		virtual void operator()(Node_CDC &node) = 0;
		virtual void operator()(Node_MultiDriver &node) = 0;
		virtual void operator()(Node_RetimingBlocker &node) = 0;
};

class ConstNodeVisitor
{
	public:
		virtual void operator()(const Node_Arithmetic &node) = 0;
		virtual void operator()(const Node_Signal2Clk &node) = 0;
		virtual void operator()(const Node_Signal2Rst &node) = 0;
		virtual void operator()(const Node_Clk2Signal &node) = 0;
		virtual void operator()(const Node_ClkRst2Signal &node) = 0;
		virtual void operator()(const Node_Compare &node) = 0;
		virtual void operator()(const Node_Constant &node) = 0;
		virtual void operator()(const Node_External &node) = 0;
		virtual void operator()(const Node_Logic &node) = 0;
		virtual void operator()(const Node_Multiplexer &node) = 0;
		virtual void operator()(const Node_Pin &node) = 0;
		virtual void operator()(const Node_PriorityConditional &node) = 0;
		virtual void operator()(const Node_Register &node) = 0;
		virtual void operator()(const Node_Rewire &node) = 0;
		virtual void operator()(const Node_Shift &node) = 0;
		virtual void operator()(const Node_Signal &node) = 0;
		virtual void operator()(const Node_SignalGenerator &node) = 0;
		virtual void operator()(const Node_SignalTap &node) = 0;
		virtual void operator()(const Node_Memory &node) = 0;
		virtual void operator()(const Node_MemPort &node) = 0;
		virtual void operator()(const Node_Default &node) = 0;
		virtual void operator()(const Node_ExportOverride &node) = 0;
		virtual void operator()(const Node_Attributes &node) = 0;
		virtual void operator()(const Node_PathAttributes &node) = 0;
		virtual void operator()(const Node_RegSpawner &node) = 0;
		virtual void operator()(const Node_RegHint &node) = 0;
		virtual void operator()(const Node_NegativeRegister &node) = 0;
		virtual void operator()(const Node_CDC &node) = 0;
		virtual void operator()(const Node_MultiDriver &node) = 0;
		virtual void operator()(const Node_RetimingBlocker &node) = 0;
};


}
