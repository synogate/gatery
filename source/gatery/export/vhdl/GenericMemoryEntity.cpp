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
#include "GenericMemoryEntity.h"

#include "AST.h"
#include "Process.h"

#include "../../hlim/Clock.h"
#include "../../hlim/coreNodes/Node_Register.h"
#include "../../hlim/coreNodes/Node_Constant.h"
#include "../../hlim/supportNodes/Node_Memory.h"
#include "../../hlim/supportNodes/Node_MemPort.h"
#include "../../hlim/postprocessing/MemoryDetector.h"
#include "../../frontend/SynthesisTool.h"

#include <map>
#include <vector>

using namespace std::string_view_literals;

namespace gtry::vhdl {

struct ClockedInstructions
{
	using PortRegisters = std::vector<std::pair<hlim::MemoryGroup::MemPort, size_t>>;
	PortRegisters readOutputRegisters;
	std::vector<hlim::MemoryGroup::MemPort> reads;
	std::vector<hlim::MemoryGroup::MemPort> readInputRegs;
	std::vector<hlim::MemoryGroup::MemPort> readConflictOverrides;

	std::vector<hlim::MemoryGroup::MemPort> writes;
};

struct AsyncReadInstruction
{
	hlim::MemoryGroup::MemPort readPort;
	bool registeredInputs;
};

struct GenericMemoryPorts
{
	utils::StableMap<RegisterConfig, ClockedInstructions> clockedInstructions;
	std::vector<AsyncReadInstruction> asyncReads;
};



GenericMemoryEntity::GenericMemoryEntity(AST &ast, const std::string &desiredName, BasicBlock *parent) : Entity(ast, desiredName, parent)
{
}

void GenericMemoryEntity::allocateInputAndOutputNames()
{
	std::set<hlim::NodePort> alreadyAllocated;
	size_t portIdx = 0;
	for (auto &port : m_memGrp->getPorts()) {
		auto addrInput = port.node->getDriver((size_t)hlim::Node_MemPort::Inputs::address);
		auto enInput = port.node->getDriver((size_t)hlim::Node_MemPort::Inputs::enable);
		auto wrEnInput = port.node->getDriver((size_t)hlim::Node_MemPort::Inputs::wrEnable);
		auto dataInput = port.node->getDriver((size_t)hlim::Node_MemPort::Inputs::wrData);

		const char *suffix;
		if (port.isReadPort() && !port.isWritePort())
			suffix = "readPort";
		else if (!port.isReadPort() && port.isWritePort())
			suffix = "writePort";
		else
			suffix = "readWritePort";

		if (addrInput.node != nullptr && hlim::getOutputConnectionType(addrInput).width > 0) {
			m_namespaceScope.allocateName(addrInput, (boost::format("%s_%d_address") % suffix % portIdx).str(), chooseDataTypeFromOutput(addrInput), CodeFormatting::SIG_ENTITY_INPUT);
			alreadyAllocated.insert(addrInput);
		}
		if (enInput.node != nullptr) {
			m_namespaceScope.allocateName(enInput, (boost::format("%s_%d_rdEn") % suffix % portIdx).str(), chooseDataTypeFromOutput(enInput), CodeFormatting::SIG_ENTITY_INPUT);
			alreadyAllocated.insert(enInput);
		}
		if (wrEnInput.node != nullptr){
			m_namespaceScope.allocateName(wrEnInput, (boost::format("%s_%d_wrEn") % suffix % portIdx).str(), chooseDataTypeFromOutput(wrEnInput), CodeFormatting::SIG_ENTITY_INPUT);
			alreadyAllocated.insert(wrEnInput);
		}
		auto wrWordEnInput = port.node->getDriver((size_t)hlim::Node_MemPort::Inputs::wrWordEnable);
		if (wrWordEnInput.node != nullptr && hlim::getOutputConnectionType(wrWordEnInput).width > 0) {
			m_namespaceScope.allocateName(wrWordEnInput, (boost::format("%s_%d_wrWordEn") % suffix % portIdx).str(), chooseDataTypeFromOutput(wrWordEnInput), CodeFormatting::SIG_ENTITY_INPUT);
			alreadyAllocated.insert(wrWordEnInput);
		}
		if (dataInput.node != nullptr && hlim::getOutputConnectionType(dataInput).width > 0) {
			m_namespaceScope.allocateName(dataInput, (boost::format("%s_%d_dataIn") % suffix % portIdx).str(), chooseDataTypeFromOutput(dataInput), CodeFormatting::SIG_ENTITY_INPUT);
			alreadyAllocated.insert(dataInput);
		}

		if (port.isReadPort()) {
			auto dataOutput = port.dataOutput;
			m_namespaceScope.allocateName(dataOutput, (boost::format("%s_%d_dataOut") % suffix % portIdx).str(), chooseDataTypeFromOutput(dataOutput), CodeFormatting::SIG_ENTITY_OUTPUT);
			alreadyAllocated.insert(dataOutput);
		}

		portIdx++;
	}

	for (auto &input : m_inputs)
		if (!alreadyAllocated.contains(input))
			m_namespaceScope.allocateName(input, findNearestDesiredName(input, m_nodeGroup), chooseDataTypeFromOutput(input), CodeFormatting::SIG_ENTITY_INPUT);

	for (auto &output : m_outputs)
		if (!alreadyAllocated.contains(output))
			m_namespaceScope.allocateName(output, findNearestDesiredName(output, m_nodeGroup), chooseDataTypeFromOutput(output), CodeFormatting::SIG_ENTITY_OUTPUT);
}

void GenericMemoryEntity::buildFrom(hlim::NodeGroup *memNodeGrp)
{
	m_nodeGroup = memNodeGrp;
	m_memGrp = dynamic_cast<hlim::MemoryGroup*>(m_nodeGroup->getMetaInfo());
	HCL_ASSERT(m_memGrp != nullptr);
	// probably not the best place to do it....
	m_namespaceScope.allocateName({.node=m_memGrp->getMemory(), .port=0}, "memory", {}, CodeFormatting::SIG_LOCAL_SIGNAL);

	for (auto node : m_nodeGroup->getNodes())
		m_ast.getMapping().assignNodeToScope(node, this);

	for (auto &port : m_memGrp->getPorts()) {
		auto addrInput = port.node->getDriver((size_t)hlim::Node_MemPort::Inputs::address);
		auto enInput = port.node->getDriver((size_t)hlim::Node_MemPort::Inputs::enable);
		auto wrEnInput = port.node->getDriver((size_t)hlim::Node_MemPort::Inputs::wrEnable);
		auto dataInput = port.node->getDriver((size_t)hlim::Node_MemPort::Inputs::wrData);
		auto dataOutput = port.dataOutput;

		if (addrInput.node != nullptr && hlim::getOutputConnectionType(addrInput).width > 0)
			m_inputs.insert(addrInput);
		auto wrWordEnInput = port.node->getDriver((size_t)hlim::Node_MemPort::Inputs::wrWordEnable);
		if (wrEnInput.node != nullptr)
			m_inputs.insert(wrEnInput);
		if (enInput.node != nullptr)
			m_inputs.insert(enInput);
		if (dataInput.node != nullptr && hlim::getOutputConnectionType(dataInput).width > 0)
			m_inputs.insert(dataInput);
		if (wrWordEnInput.node != nullptr && hlim::getOutputConnectionType(wrWordEnInput).width > 0)
			m_inputs.insert(wrWordEnInput);


		if (port.isReadPort()) {
			m_outputs.insert(dataOutput);

			for (auto reg : port.dedicatedReadLatencyRegisters) {
				m_inputClocks.insert(reg->getClocks()[0]->getClockPinSource());

				auto enInput = reg->getDriver((size_t)hlim::Node_Register::Input::ENABLE);
				if (enInput.node != nullptr) 
					m_inputs.insert(enInput);

				auto resetValue = reg->getNonSignalDriver((size_t)hlim::Node_Register::Input::RESET_VALUE);
				if (resetValue.node != nullptr) {
					//m_inputs.insert(resetValue);

					if (reg->getClocks()[0]->getRegAttribs().resetType != hlim::RegisterAttributes::ResetType::NONE)
						m_inputResets.insert(reg->getClocks()[0]->getResetPinSource());
				}
			}
		}

		if (port.isWritePort())
			m_inputClocks.insert(port.node->getClocks()[0]->getClockPinSource());
	}
#if 1
	// Determine if we need a shared variable for the memory.
	// Multiple processes writing to the same signal is illegal in VHDL,
	// so we use a shared variable when write ports have different clocks.
	std::set<hlim::Clock*> writeClocks;
	for (auto &port : m_memGrp->getPorts())
		if (port.isWritePort())
			writeClocks.insert(port.node->getClocks()[0]);
	m_needsSharedVariable = writeClocks.size() > 1;
#else
	m_needsSharedVariable = m_memGrp->getPorts().size() > 1;
#endif
}


void GenericMemoryEntity::formatReadPortOutputSignalName(std::ostream &stream, const hlim::MemoryGroup::MemPort &readPort, size_t regIndex) const
{
	const auto &outputDecl = m_namespaceScope.get(readPort.dataOutput);
	if (regIndex+1 >= readPort.dedicatedReadLatencyRegisters.size())
		stream << outputDecl.name;
	else
		stream << outputDecl.name << "_outputReg_" << regIndex;
}


void GenericMemoryEntity::formatReadPortInputRegisterSignalName(std::ostream &stream, const hlim::NodePort &driver) const
{
	const auto &outputDecl = m_namespaceScope.get(driver);
	stream << outputDecl.name << "_inputReg";
}

void GenericMemoryEntity::writeLocalSignalsVHDL(std::ostream &stream)
{
	CodeFormatting &cf = m_ast.getCodeFormatting();

	Entity::writeLocalSignalsVHDL(stream);

	auto memorySize = m_memGrp->getMemory()->getSize();

	std::set<size_t> portSizes;

	for (auto &rp : m_memGrp->getReadPorts())
		portSizes.insert(rp.node->getBitWidth());


	HCL_ASSERT_HINT(portSizes.size() == 1, "Memory with mixed port sizes not yet supported!");

	size_t wordSize = *portSizes.begin();

	HCL_ASSERT_HINT(memorySize % wordSize == 0, "Memory size is not a multiple of the word size!");


	cf.indent(stream, 1);
	stream << "CONSTANT WORD_WIDTH : integer := " << wordSize << ";\n";
	cf.indent(stream, 1);
	stream << "CONSTANT NUM_WORDS : integer := " << memorySize/wordSize << ";\n";

	cf.indent(stream, 1);
	stream << "SUBTYPE mem_word_type IS UNSIGNED(WORD_WIDTH-1 downto 0);\n";
	cf.indent(stream, 1);
	stream << "TYPE mem_type IS array(NUM_WORDS-1 downto 0) of mem_word_type;\n";

	cf.indent(stream, 1);
	if (m_needsSharedVariable)
		stream << "SHARED VARIABLE memory : mem_type";
	else
		stream << "SIGNAL memory : mem_type";


	if (m_memGrp->getMemory()->requiresPowerOnInitialization())
	{
		const auto &powerOnState = m_memGrp->getMemory()->getPowerOnState();

		stream << " := (\n";

		auto memorySize = m_memGrp->getMemory()->getSize();

		std::set<size_t> portSizes;

		for (auto &p : m_memGrp->getPorts())
			portSizes.insert(p.node->getBitWidth());

		HCL_ASSERT_HINT(portSizes.size() == 1, "Memory with mixed port sizes not yet supported!");

		size_t wordSize = *portSizes.begin();

		HCL_ASSERT_HINT(memorySize % wordSize == 0, "Memory size is not a multiple of the word size!");

		unsigned indent = 2;

		for (auto i : utils::Range(memorySize/wordSize)) {
			bool anyDefined = false;
			for (auto j : utils::Range(wordSize))
				if (powerOnState.get(gtry::sim::DefaultConfig::DEFINED, i*wordSize + wordSize-1-j)) {
					anyDefined = true;
					break;
				}

			if (anyDefined) {
				cf.indent(stream, indent);
				stream << i << " => \"";
				for (auto j : utils::Range(wordSize)) {
					bool defined = powerOnState.get(gtry::sim::DefaultConfig::DEFINED, i*wordSize + wordSize-1-j);
					bool value = powerOnState.get(gtry::sim::DefaultConfig::VALUE, i*wordSize + wordSize-1-j);
					if (!defined)
						stream << 'X';
					else
						if (value)
							stream << '1';
						else
							stream << '0';
				}
				stream << "\",\n";
			}
		}
		cf.indent(stream, 2);
		stream << "others => (others => 'X'))";
	}

	stream << ";\n";

	/*
	Xilinx:
	attribute ram_style : string;
	attribute ram_style of myram : signal is "distributed";

	attribute rom_style : string;
	attribute rom_style of myrom : signal is "distributed";

	attribute rw_addr_collision : string;
	attribute rw_addr_collision of my_ram : signal is "yes";



	*/


	hlim::ResolvedAttributes resolvedAttribs;
	m_ast.getSynthesisTool().resolveAttributes(m_memGrp->getMemory()->getAttribs(), resolvedAttribs);


	std::map<std::string, hlim::AttribValue> alreadyDeclaredAttribs;
	// write out all memory attribs
	for (const auto &attrib : resolvedAttribs) {
		auto it = alreadyDeclaredAttribs.find(attrib.first);
		if (it == alreadyDeclaredAttribs.end()) {
			alreadyDeclaredAttribs[attrib.first] = attrib.second;

			cf.indent(stream, 1);
			stream << "ATTRIBUTE " << attrib.first << " : " << attrib.second.type << ';' << std::endl;
		} else
			HCL_DESIGNCHECK_HINT(it->second.type == attrib.second.type, "Same attribute can't have different types!");

		cf.indent(stream, 1);
		stream << "ATTRIBUTE " << attrib.first << " of memory : ";
		stream << "SIGNAL is " << attrib.second.value << ';' << std::endl;
	}


	for (auto &rp : m_memGrp->getReadPorts()) {
		if (!rp.dedicatedReadLatencyRegisters.empty())
			for (auto i : utils::Range<size_t>(rp.dedicatedReadLatencyRegisters.size()-1)) {
				const auto &decl = m_namespaceScope.get(rp.dataOutput);

				cf.indent(stream, 1);
				stream << "SIGNAL ";
				formatReadPortOutputSignalName(stream, rp, i);
				stream << " : ";
				cf.formatConnectionType(stream, decl);

				auto& reg = rp.dedicatedReadLatencyRegisters[i];
				if (reg->getDriver(hlim::Node_Register::RESET_VALUE).node != nullptr && reg->getClocks()[0]->getRegAttribs().initializeRegs) {

					auto* constant = dynamic_cast<hlim::Node_Constant*>(reg->getNonSignalDriver(hlim::Node_Register::RESET_VALUE).node);
					HCL_ASSERT_HINT(constant != nullptr, "Register reset value must resolve to constant!");

					stream << " := ";

					const auto& conType = constant->getOutputConnectionType(0);
					char sep = '"';
					if (conType.isBool())
						sep = '\'';

					stream << sep;
					stream << constant->getValue();
					stream << sep;
				}

				stream << "; "<< std::endl;
			}

		if (!m_memGrp->readUnorderedToAllOthersWrite(rp) && m_memGrp->canReadAfterAllOthersWrite(rp)) {
			auto addrInput = rp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::address);
			const auto &decl = m_namespaceScope.get(addrInput);

			cf.indent(stream, 1);
			stream << "SIGNAL ";
			formatReadPortInputRegisterSignalName(stream, addrInput);
			stream << " : ";
			cf.formatConnectionType(stream, decl);
			stream << "; "<< std::endl;
		}
	}
}


void GenericMemoryEntity::generateReadPortConflictDetectionExpression(std::ostream &stream, const hlim::MemoryGroup::MemPort &readPort)
{
	CodeFormatting &cf = m_ast.getCodeFormatting();

	hlim::NodePort enablePort = {};
	if (!readPort.dedicatedReadLatencyRegisters.empty())
		enablePort = readPort.dedicatedReadLatencyRegisters[0]->getDriver((unsigned)hlim::Node_Register::Input::ENABLE);

	if (enablePort.node) {
		auto enable = m_namespaceScope.get(enablePort).name;
		stream << enable << " = '1' AND (";
	}


	if (readPort.addressWidth() == 0_b) {
		// Special handling for depth-1 memories, which don't have an address port. Any write enables active at the same time as the read enable are a conflict.

		bool firstCondition = true;
		for (const auto &wp : m_memGrp->getWritePorts()) {
			auto wrEnablePort = wp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::wrEnable);

			if (wrEnablePort.node != nullptr) {
				if (!firstCondition)
					stream << " OR ";
				firstCondition = false;

				stream << m_namespaceScope.get(wrEnablePort).name << " = '1'";
			}
		}
	} else {
		auto addrPort = readPort.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::address);
		auto addrWidth = hlim::getOutputConnectionType(addrPort).width;
		auto addr = m_namespaceScope.get(addrPort).name;

		bool firstCondition = true;
		for (const auto &wp : m_memGrp->getWritePorts()) {
			auto wrEnablePort = wp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::wrEnable);
			auto wrAddrPort = wp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::address);

			auto wrAddrWidth = hlim::getOutputConnectionType(wrAddrPort).width;
			auto wrAddr = m_namespaceScope.get(wrAddrPort).name;

			if (!firstCondition)
				stream << " OR ";
			firstCondition = false;

			stream << '(';
			if (wrEnablePort.node != nullptr)
				stream << m_namespaceScope.get(wrEnablePort).name << " = '1' AND ";

			stream << addr << " = " << wrAddr << ')';
		}
	}

	if (enablePort.node)
		stream << ')';

}

void GenericMemoryEntity::openResetCondition(std::ostream &stream, unsigned indent, const std::string_view resetName, const RegisterConfig &reset)
{
	CodeFormatting &cf = m_ast.getCodeFormatting();

	cf.indent(stream, indent);
	const char resetValue = reset.resetHighActive ? '1' : '0';
	stream << "IF ("<< resetName << " = '" << resetValue << "') THEN\n";
}


void GenericMemoryEntity::exportResetOutputReg(std::ostream &stream, unsigned indent, const hlim::MemoryGroup::MemPort &readPort, size_t regIndex)
{
	CodeFormatting &cf = m_ast.getCodeFormatting();

	auto &regNode = readPort.dedicatedReadLatencyRegisters[regIndex];
	auto reset = regNode->getNonSignalDriver((unsigned)hlim::Node_Register::Input::RESET_VALUE);

	if (reset.node != nullptr) {
		auto *constReset = dynamic_cast<hlim::Node_Constant*>(reset.node);
		HCL_DESIGNCHECK_HINT(constReset, "Resets of registers must be constants upon export!");

		const auto &outputDecl = m_namespaceScope.get(readPort.dataOutput);

		cf.indent(stream, indent);
		formatReadPortOutputSignalName(stream, readPort, regIndex);
		stream << " <= ";
		formatConstant(stream, constReset, outputDecl.dataType);
		stream << ";" << std::endl;
	}
}


void GenericMemoryEntity::exportWrite(std::ostream &stream, unsigned indent, const hlim::MemoryGroup::MemPort &writePort)
{
	CodeFormatting &cf = m_ast.getCodeFormatting();

	auto wrEnablePort = writePort.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::wrEnable);
	if (wrEnablePort.node != nullptr) {
		cf.indent(stream, indent);
		stream << "IF ("<< m_namespaceScope.get(wrEnablePort).name << " = '1') THEN\n";
		indent++;
	}

	auto addrPort = writePort.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::address);
	auto dataPort = writePort.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::wrData);

	const auto &dataDecl = m_namespaceScope.get(dataPort);
	auto memAssign = m_needsSharedVariable ? " := "sv : " <= "sv;

	size_t wrMaskWidth = writePort.node->getWriteMaskWidth();

	if (wrMaskWidth == 0) {
		cf.indent(stream, indent);
		if (hlim::getOutputConnectionType(addrPort).width > 0)
			stream << "memory(to_integer(" << m_namespaceScope.get(addrPort).name << "))" << memAssign;
		else
			stream << "memory(0)" << memAssign;
		if (dataDecl.dataType != VHDLDataType::UNSIGNED) {
			cf.formatDataType(stream, VHDLDataType::UNSIGNED);
			stream << '(' << dataDecl.name << ");\n";
		} else
			stream << dataDecl.name << ";\n";
	} else {
		auto wrWordEnPort = writePort.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::wrWordEnable);
		const auto &maskDecl = m_namespaceScope.get(wrWordEnPort);
		size_t granularity = writePort.node->getBitWidth() / wrMaskWidth;

		cf.indent(stream, indent);
		stream << "FOR i IN 0 TO " << wrMaskWidth - 1 << " LOOP\n";
		indent++;

		cf.indent(stream, indent);
		stream << "IF (" << maskDecl.name << "(i) = '1') THEN\n";
		indent++;

		cf.indent(stream, indent);
		if (hlim::getOutputConnectionType(addrPort).width > 0)
			stream << "memory(to_integer(" << m_namespaceScope.get(addrPort).name << "))";
		else
			stream << "memory(0)";
		stream << "((i+1)*" << granularity << "-1 downto i*" << granularity << ")" << memAssign;
		if (dataDecl.dataType != VHDLDataType::UNSIGNED) {
			cf.formatDataType(stream, VHDLDataType::UNSIGNED);
			stream << '(' << dataDecl.name << ")";
		} else
			stream << dataDecl.name;
		stream << "((i+1)*" << granularity << "-1 downto i*" << granularity << ");\n";

		indent--;
		cf.indent(stream, indent);
		stream << "END IF;\n";

		indent--;
		cf.indent(stream, indent);
		stream << "END LOOP;\n";
	}

	if (wrEnablePort.node != nullptr) {
		indent--;
		cf.indent(stream, indent);
		stream << "END IF;\n";
	}
}

void GenericMemoryEntity::exportRead(std::ostream &stream, unsigned indent, const hlim::MemoryGroup::MemPort &readPort, bool registeredInputs, bool useEnablePort)
{
	CodeFormatting &cf = m_ast.getCodeFormatting();
	auto addrPort = readPort.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::address);
	auto dataPort = readPort.dataOutput;

	bool openedEnableScope = false;
	if (useEnablePort && !readPort.dedicatedReadLatencyRegisters.empty()) {
		auto enablePort = readPort.dedicatedReadLatencyRegisters[0]->getDriver((unsigned)hlim::Node_Register::Input::ENABLE);
		if (enablePort.node != nullptr) {
			cf.indent(stream, indent);
			stream << "IF ("<< m_namespaceScope.get(enablePort).name << " = '1') THEN\n";
			indent++;
			openedEnableScope = true;
		}
	}

	const auto &outputDecl = m_namespaceScope.get(dataPort);

	cf.indent(stream, indent);
	formatReadPortOutputSignalName(stream, readPort, 0);

	stream << " <= ";

	if (outputDecl.dataType != VHDLDataType::UNSIGNED) {
		cf.formatDataType(stream, outputDecl.dataType);
		stream << '(';
	}

	if (hlim::getOutputConnectionType(addrPort).width > 0) {
		stream << "memory(to_integer(";
		if (registeredInputs)
			formatReadPortInputRegisterSignalName(stream, addrPort);
		else
			stream << m_namespaceScope.get(addrPort).name;
		stream << "));\n";
	} else
		stream << "memory(0);\n";

	if (outputDecl.dataType != VHDLDataType::UNSIGNED) {
		stream << ')';
	}

	if (openedEnableScope) {
		indent--;
		cf.indent(stream, indent);
		stream << "END IF;\n";
	}
}

void GenericMemoryEntity::exportReadInputReg(std::ostream &stream, unsigned indent, const hlim::MemoryGroup::MemPort &readPort)
{
	CodeFormatting &cf = m_ast.getCodeFormatting();

	auto addrPort = readPort.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::address);
	const auto &outputDecl = m_namespaceScope.get(addrPort);

	cf.indent(stream, indent);
	formatReadPortInputRegisterSignalName(stream, addrPort);
	stream << " <= " << outputDecl.name << ";\n";
}

void GenericMemoryEntity::exportReadOutputReg(std::ostream &stream, unsigned indent, const hlim::MemoryGroup::MemPort &readPort, size_t regIndex)
{
	CodeFormatting &cf = m_ast.getCodeFormatting();
	auto addrPort = readPort.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::address);
	auto dataPort = readPort.dataOutput;

	bool openedEnableScope = false;
	auto enablePort = readPort.dedicatedReadLatencyRegisters[regIndex]->getDriver((unsigned)hlim::Node_Register::Input::ENABLE);
	if (enablePort.node != nullptr) {
		cf.indent(stream, indent);
		stream << "IF ("<< m_namespaceScope.get(enablePort).name << " = '1') THEN\n";
		indent++;
		openedEnableScope = true;
	}

	const auto &outputDecl = m_namespaceScope.get(dataPort);

	cf.indent(stream, indent);
	formatReadPortOutputSignalName(stream, readPort, regIndex);

	stream << " <= ";

	formatReadPortOutputSignalName(stream, readPort, regIndex-1);

	stream << ';' << std::endl;

	if (openedEnableScope) {
		indent--;
		cf.indent(stream, indent);
		stream << "END IF;\n";
	}
}

void GenericMemoryEntity::exportReadConflictOverride(std::ostream &stream, unsigned indent, const hlim::MemoryGroup::MemPort &readPort)
{
	CodeFormatting &cf = m_ast.getCodeFormatting();

	cf.indent(stream, indent);
	stream << "IF (";
	generateReadPortConflictDetectionExpression(stream, readPort);
	stream << ") THEN\n";
	indent++;

	cf.indent(stream, indent);
	formatReadPortOutputSignalName(stream, readPort, 0);
	stream << " <= (others => 'X');" << std::endl;

	indent--;
	cf.indent(stream, indent);
	stream << "END IF;\n";
}


void GenericMemoryEntity::writeStatementsVHDL(std::ostream &stream, unsigned indent)
{
	CodeFormatting &cf = m_ast.getCodeFormatting();

	GenericMemoryPorts memoryPorts = groupPorts();

	for (auto &[regConfig, clockedInstructions] : memoryPorts.clockedInstructions) {
		std::string clockName = m_namespaceScope.getClock(regConfig.clock).name;
		std::string resetName;
		if (regConfig.reset != nullptr)
			resetName = m_namespaceScope.getReset(regConfig.reset).name;

		unsigned indent = 1;
		cf.indent(stream, indent);
		if (regConfig.resetType == hlim::RegisterAttributes::ResetType::ASYNCHRONOUS)
			stream << "PROCESS(" << clockName << ", " << resetName << ")" << std::endl;
		else
			stream << "PROCESS(" << clockName << ")" << std::endl;

		cf.indent(stream, indent);
		stream << "BEGIN\n";

		indent++;

		if (regConfig.resetType == hlim::RegisterAttributes::ResetType::ASYNCHRONOUS) {

			openResetCondition(stream, indent, resetName, regConfig);

			for (auto &rp : clockedInstructions.reads)
				exportResetOutputReg(stream, indent+1, rp, 0);

			for (auto &outRegs : clockedInstructions.readOutputRegisters)
				exportResetOutputReg(stream, indent+1, outRegs.first, outRegs.second);
			
			cf.indent(stream, indent);
			stream << "ELSIF";
		} else {
			cf.indent(stream, indent);
			stream << "IF";
		}

		switch (regConfig.triggerEvent) {
			case hlim::Clock::TriggerEvent::RISING:
				stream << " (rising_edge(" << clockName << ")) THEN\n";
			break;
			case hlim::Clock::TriggerEvent::FALLING:
				stream << " (falling_edge(" << clockName << ")) THEN\n";
			break;
			case hlim::Clock::TriggerEvent::RISING_AND_FALLING:
				stream << " (" << clockName << "'event) THEN\n";
			break;
			default:
				HCL_ASSERT_HINT(false, "Unhandled case");
		}
		indent++;

		for (auto &wp : clockedInstructions.writes)
			exportWrite(stream, indent, wp);

		if (regConfig.resetType == hlim::RegisterAttributes::ResetType::SYNCHRONOUS) {

			openResetCondition(stream, indent, resetName, regConfig);

			for (auto &rp : clockedInstructions.reads)
				exportResetOutputReg(stream, indent+1, rp, 0);

			for (auto &outRegs : clockedInstructions.readOutputRegisters)
				exportResetOutputReg(stream, indent+1, outRegs.first, outRegs.second);
			
			cf.indent(stream, indent);
			stream << "ELSE" << std::endl;

			indent++;
		}

		for (auto &rp : clockedInstructions.readInputRegs)
			exportReadInputReg(stream, indent, rp);

		for (auto &rp : clockedInstructions.reads)
			exportRead(stream, indent, rp, false, true);

		for (auto &rp : clockedInstructions.readConflictOverrides)
			exportReadConflictOverride(stream, indent, rp);

		for (auto &outRegs : clockedInstructions.readOutputRegisters)
			exportReadOutputReg(stream, indent, outRegs.first, outRegs.second);

		if (regConfig.resetType == hlim::RegisterAttributes::ResetType::SYNCHRONOUS) {
			indent--;
			cf.indent(stream, indent);
			stream << "END IF;\n";
		}

		indent--;
		cf.indent(stream, indent);
		stream << "END IF;\n";

		indent--;
		cf.indent(stream, indent);
		stream << "END PROCESS;\n\n";
	}

	if (!memoryPorts.asyncReads.empty()) {
		cf.indent(stream, indent);
		stream << "PROCESS(all)\n";
		cf.indent(stream, indent);
		stream << "BEGIN\n";

		for (auto &r : memoryPorts.asyncReads)
			exportRead(stream, indent+1, r.readPort, r.registeredInputs, false);

		cf.indent(stream, indent);
		stream << "END PROCESS;\n\n";
	}
}

GenericMemoryPorts GenericMemoryEntity::groupPorts() const
{
	GenericMemoryPorts memPorts;

	for (auto &wp : m_memGrp->getWritePorts()) {
		auto &c = wp.node->getClocks()[0];
		memPorts.clockedInstructions[RegisterConfig::fromClock(c, false)].writes.push_back(wp);
	}


	for (auto &rp : m_memGrp->getReadPorts())
		if (!rp.dedicatedReadLatencyRegisters.empty()) {
			auto &regNode = rp.dedicatedReadLatencyRegisters[0];
			auto &c = regNode->getClocks()[0];
			bool hasReset = regNode->getNonSignalDriver(hlim::Node_Register::RESET_VALUE).node != nullptr;
			auto &clockedInstructions = memPorts.clockedInstructions[RegisterConfig::fromClock(c, hasReset)];

			if (m_memGrp->readUnorderedToAllOthersWrite(rp)) {
				clockedInstructions.reads.push_back(rp);
				clockedInstructions.readConflictOverrides.push_back(rp);
			} else if (m_memGrp->canReadBeforeAllOthersWrite(rp)) {
				clockedInstructions.reads.push_back(rp);
			} else if (m_memGrp->canReadAfterAllOthersWrite(rp)) {
				clockedInstructions.readInputRegs.push_back(rp);
				memPorts.asyncReads.push_back({.readPort = rp, .registeredInputs = true});
			} else 
				HCL_ASSERT_HINT(false, "Can't express memories with reads happening after some writes but before others!");

			for (auto i : utils::Range<size_t>(1, rp.dedicatedReadLatencyRegisters.size())) {
				auto &regNode = rp.dedicatedReadLatencyRegisters[i];
				auto &c = regNode->getClocks()[0];
				bool hasReset = regNode->getNonSignalDriver(hlim::Node_Register::RESET_VALUE).node != nullptr;
				memPorts.clockedInstructions[RegisterConfig::fromClock(c, hasReset)].readOutputRegisters.push_back({rp, i});
			}
		} else
			memPorts.asyncReads.push_back({.readPort = rp, .registeredInputs = false});

	return memPorts;
}



}
