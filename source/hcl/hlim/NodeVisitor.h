/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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

namespace hcl::core::hlim {

class Node_Arithmetic;
class Node_Clk2Signal;
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
    
class NodeVisitor
{
    public:
        virtual void operator()(Node_Arithmetic &node) = 0;
        virtual void operator()(Node_Clk2Signal &node) = 0;
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
};

class ConstNodeVisitor
{
    public:
        virtual void operator()(const Node_Arithmetic &node) = 0;
        virtual void operator()(const Node_Clk2Signal &node) = 0;
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
};


}
