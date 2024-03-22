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
#include "Node_External.h"

namespace gtry::hlim {

void Node_External::copyBaseToClone(BaseNode *copy) const
{
	BaseNode::copyBaseToClone(copy);
	Node_External *other = (Node_External*)copy;

	other->m_isEntity = m_isEntity;
	other->m_libraryName = m_libraryName;
	other->m_packageName = m_packageName;
	other->m_genericParameters = m_genericParameters;
	other->m_clockNames = m_clockNames;
	other->m_resetNames = m_resetNames;

	other->m_inputPorts = m_inputPorts;
	other->m_outputPorts = m_outputPorts;

	other->m_inputIsEnable = m_inputIsEnable;
}

void Node_External::resizeIOPorts(size_t numInputs, size_t numOutputs)
{
	resizeInputs(numInputs);
	resizeOutputs(numOutputs);
}


void Node_External::resizeInputs(size_t num)
{
	BaseNode::resizeInputs(num);
	m_inputPorts.resize(num);
}

void Node_External::resizeOutputs(size_t num)
{
	BaseNode::resizeOutputs(num);
	m_outputPorts.resize(num);
}




void Node_External::declOutputBitVector(size_t idx, std::string name, size_t width, std::string componentWidth, BitFlavor flavor)
{
	HCL_DESIGNCHECK_HINT(idx < getNumOutputPorts(), "Invalid output idx");
	m_outputPorts[idx].name = std::move(name);
	m_outputPorts[idx].componentWidth = std::move(componentWidth);
	m_outputPorts[idx].instanceWidth = width;
	m_outputPorts[idx].isVector = true;
	m_outputPorts[idx].flavor = flavor;

	setOutputConnectionType(idx, {
		.type = hlim::ConnectionType::BITVEC,
		.width = width,
	});
}

void Node_External::declOutputBit(size_t idx, std::string name, BitFlavor flavor)
{
	HCL_DESIGNCHECK_HINT(idx < getNumOutputPorts(), "Invalid output idx");
	m_outputPorts[idx].name = std::move(name);
	m_outputPorts[idx].isVector = false;
	m_outputPorts[idx].flavor = flavor;

	setOutputConnectionType(idx, {
		.type = hlim::ConnectionType::BOOL,
		.width = 1,
	});
}

void Node_External::declInputBitVector(size_t idx, std::string name, size_t width, std::string componentWidth, BitFlavor flavor)
{
	HCL_DESIGNCHECK_HINT(idx < getNumInputPorts(), "Invalid input idx");
	m_inputPorts[idx].name = std::move(name);
	if (!componentWidth.empty())
		m_inputPorts[idx].componentWidth = std::move(componentWidth);
	m_inputPorts[idx].instanceWidth = width;
	m_inputPorts[idx].isVector = true;
	m_inputPorts[idx].flavor = flavor;
}

void Node_External::declInputBit(size_t idx, std::string name, BitFlavor flavor)
{
	HCL_DESIGNCHECK_HINT(idx < getNumInputPorts(), "Invalid input idx");
	m_inputPorts[idx].name = std::move(name);
	m_inputPorts[idx].isVector = false;
	m_inputPorts[idx].flavor = flavor;
}

void Node_External::changeInputWidth(size_t idx, size_t width)
{
	HCL_DESIGNCHECK_HINT(m_inputPorts[idx].isVector, "Input not declared as vector!");
	HCL_DESIGNCHECK_HINT(getDriver(idx).node == nullptr, "Input is already bound!");
	m_inputPorts[idx].instanceWidth = width;
}

void Node_External::changeOutputWidth(size_t idx, size_t width)
{
	HCL_DESIGNCHECK_HINT(m_outputPorts[idx].isVector, "Output not declared as vector!");
	HCL_DESIGNCHECK_HINT(getDirectlyDriven(idx).empty(), "Output is already bound!");
	m_outputPorts[idx].instanceWidth = width;
	setOutputConnectionType(idx, {
		.type = hlim::ConnectionType::BITVEC,
		.width = width,
	});	
}


void Node_External::declBidirPort(size_t inputIdx, size_t outputIdx)
{
	HCL_DESIGNCHECK_HINT(m_inputPorts[inputIdx] == m_outputPorts[outputIdx], "For two ports of an external node to be declared partners of a bidirectional signal, they must have identical attributes!");
	m_inputPorts[inputIdx].bidirPartner = outputIdx;
	m_outputPorts[outputIdx].bidirPartner = inputIdx;
}


std::string Node_External::getInputName(size_t idx) const
{
	HCL_DESIGNCHECK_HINT(idx < getNumInputPorts(), "Invalid input idx");
	return m_inputPorts[idx].name;
}

std::string Node_External::getOutputName(size_t idx) const
{
	HCL_DESIGNCHECK_HINT(idx < getNumOutputPorts(), "Invalid output idx");
	return m_outputPorts[idx].name;
}

std::string Node_External::attemptInferOutputName(size_t outputPort) const
{
	return m_name + '_' + getOutputName(outputPort);
}

bool Node_External::inputIsEnable(size_t inputPort) const
{
	return m_inputIsEnable.contains(inputPort);
}

void Node_External::declareInputIsEnable(size_t inputPort)
{
	m_inputIsEnable.insert(inputPort);
}


}