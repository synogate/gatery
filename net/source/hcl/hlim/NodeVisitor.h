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
class Node_Signal;
class Node_SignalGenerator;
class Node_SignalTap;
    
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
        virtual void operator()(Node_SignalGenerator &node) = 0;
        virtual void operator()(Node_SignalTap &node) = 0;
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
        virtual void operator()(const Node_Signal &node) = 0;
        virtual void operator()(const Node_SignalGenerator &node) = 0;
        virtual void operator()(const Node_SignalTap &node) = 0;
};


}
