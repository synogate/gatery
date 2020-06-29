#include "Node_PriorityConditional.h"


namespace hcl::core::hlim {

void Node_PriorityConditional::connectDefault(const NodePort &port) 
{ 
    NodeIO::connectInput(inputPortDefault(), port); 
    setOutputConnectionType(0, port.node->getOutputConnectionType(port.port)); 
}

void Node_PriorityConditional::disconnectDefault() 
{ 
    NodeIO::disconnectInput(inputPortDefault()); 
}
    
    
void Node_PriorityConditional::connectInput(size_t choice, const NodePort &condition, const NodePort &value)
{
    NodeIO::connectInput(inputPortChoiceCondition(choice), condition); 
    NodeIO::connectInput(inputPortChoiceValue(choice), value); 
    setOutputConnectionType(0, value.node->getOutputConnectionType(value.port)); 
}

void Node_PriorityConditional::addInput(const NodePort &condition, const NodePort &value)
{    
    size_t choice = getNumChoices();
    resizeInputs(1 + 2*(choice+1));
    connectInput(choice, condition, value);
}

void Node_PriorityConditional::disconnectInput(size_t choice)
{
    NodeIO::disconnectInput(inputPortChoiceCondition(choice));
    NodeIO::disconnectInput(inputPortChoiceValue(choice));
}


void Node_PriorityConditional::simulateEvaluate(sim::SimulatorCallbacks &simCallbacks, sim::DefaultBitVectorState &state, const size_t *internalOffsets, const size_t *inputOffsets, const size_t *outputOffsets) const
{
    for (auto choice : utils::Range(getNumChoices())) {
        auto conditionDriver = getNonSignalDriver(inputPortChoiceCondition(choice));
        if (conditionDriver.node == nullptr) {
            state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
            return;
        }
        
        std::uint64_t conditionDefined = state.extractNonStraddling(sim::DefaultConfig::DEFINED, inputOffsets[inputPortChoiceCondition(choice)], 1);
        std::uint64_t condition = state.extractNonStraddling(sim::DefaultConfig::VALUE, inputOffsets[inputPortChoiceCondition(choice)], 1);
        
        if (!conditionDefined) {
            state.setRange(sim::DefaultConfig::DEFINED, outputOffsets[0], getOutputConnectionType(0).width, false);
            return;
        }
        
        if (condition) {
            state.copyRange(outputOffsets[0], state, inputOffsets[inputPortChoiceValue(choice)], getOutputConnectionType(0).width);
            return;
        }
    }
    state.copyRange(outputOffsets[0], state, inputOffsets[inputPortDefault()], getOutputConnectionType(0).width);
}



std::string Node_PriorityConditional::getTypeName() const 
{ 
    return "PrioConditional"; 
}

void Node_PriorityConditional::assertValidity() const 
{ 
    
}

std::string Node_PriorityConditional::getInputName(size_t idx) const 
{ 
    if (idx == 0) 
        return "default"; 
    if (idx % 2 == 1)
        return (boost::format("condition_%d") % ((idx-1)/2)).str(); 
    return (boost::format("value_%d") % ((idx-1)/2)).str(); 
}

std::string Node_PriorityConditional::getOutputName(size_t idx) const 
{ 
    return "out"; 
    
}


}
