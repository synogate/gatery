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

#include "../../hlim/Clock.h"
#include "../../hlim/coreNodes/Node_Register.h"
#include "../../hlim/coreNodes/Node_Constant.h"
#include "../../hlim/supportNodes/Node_Memory.h"
#include "../../hlim/supportNodes/Node_MemPort.h"
#include "../../hlim/postprocessing/MemoryDetector.h"
#include "../../frontend/SynthesisTool.h"

#include <map>
#include <vector>

namespace gtry::vhdl {

GenericMemoryEntity::GenericMemoryEntity(AST &ast, const std::string &desiredName, BasicBlock *parent) : Entity(ast, desiredName, parent)
{
}

void GenericMemoryEntity::buildFrom(hlim::NodeGroup *memNodeGrp)
{
	m_memNodeGrp = memNodeGrp;
	m_memGrp = dynamic_cast<hlim::MemoryGroup*>(m_memNodeGrp->getMetaInfo());
	HCL_ASSERT(m_memGrp != nullptr);
	// probably not the best place to do it....
	m_namespaceScope.allocateName({.node=m_memGrp->getMemory(), .port=0}, "memory", {}, CodeFormatting::SIG_LOCAL_SIGNAL);

	for (auto node : m_memNodeGrp->getNodes())
		m_ast.getMapping().assignNodeToScope(node, this);

	for (auto &wp : m_memGrp->getWritePorts()) {
		auto addrInput = wp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::address);
		auto enInput = wp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::enable);
		auto wrEnInput = wp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::wrEnable);
		auto dataInput = wp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::wrData);

		HCL_ASSERT_HINT(enInput == wrEnInput, "For now I don't want to mix read and write ports, so wrEn == en always.");

		if (addrInput.node != nullptr)
			m_inputs.insert(addrInput);
		if (enInput.node != nullptr)
			m_inputs.insert(enInput);
		if (dataInput.node != nullptr)
			m_inputs.insert(dataInput);

		m_inputClocks.insert(wp.node->getClocks()[0]->getClockPinSource());
	}
	for (auto &rp : m_memGrp->getReadPorts()) {
		auto addrInput = rp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::address);
		auto enInput = rp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::enable);
		auto dataOutput = rp.dataOutput;

		if (addrInput.node != nullptr)
			m_inputs.insert(addrInput);
		if (enInput.node != nullptr)
			m_inputs.insert(enInput);

		m_outputs.insert(dataOutput);

		for (auto reg : rp.dedicatedReadLatencyRegisters) {
			m_inputClocks.insert(reg->getClocks()[0]->getClockPinSource());

			auto enInput = reg->getDriver((unsigned)hlim::Node_Register::Input::ENABLE);
			if (enInput.node != nullptr) 
				m_inputs.insert(enInput);

			auto resetValue = reg->getDriver((unsigned)hlim::Node_Register::Input::RESET_VALUE);
			if (enInput.node != nullptr && resetValue.node != nullptr) {
				m_inputs.insert(resetValue);

				if (reg->getClocks()[0]->getRegAttribs().resetType != hlim::RegisterAttributes::ResetType::NONE)
					m_inputResets.insert(reg->getClocks()[0]->getResetPinSource());
			}
		}
	}
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
	stream << "SIGNAL memory : mem_type";


	if (m_memGrp->getMemory()->requiresPowerOnInitialization())
	{
		const auto &powerOnState = m_memGrp->getMemory()->getPowerOnState();

		stream << " := (\n";

		auto memorySize = m_memGrp->getMemory()->getSize();

		std::set<size_t> portSizes;

		for (auto &wp : m_memGrp->getWritePorts())
			portSizes.insert(wp.node->getBitWidth());

		for (auto &rp : m_memGrp->getReadPorts())
			portSizes.insert(rp.node->getBitWidth());

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
				stream << "SIGNAL " << decl.name << "_outputReg_" << i << " : ";
				cf.formatConnectionType(stream, decl);

				auto& reg = rp.dedicatedReadLatencyRegisters[i];
				if (reg->getDriver(hlim::Node_Register::RESET_VALUE).node != nullptr && reg->getClocks()[0]->getRegAttribs().initializeRegs) {

					auto* constant = dynamic_cast<hlim::Node_Constant*>(reg->getNonSignalDriver(hlim::Node_Register::RESET_VALUE).node);
					HCL_ASSERT_HINT(constant != nullptr, "Register reset value must resolve to constant!");

					stream << " := ";

					const auto& conType = constant->getOutputConnectionType(0);
					char sep = '"';
					if (conType.interpretation == hlim::ConnectionType::BOOL)
						sep = '\'';

					stream << sep;
					stream << constant->getValue();
					stream << sep;
				}

				stream << "; "<< std::endl;
			}
	}
}

void GenericMemoryEntity::writeStatementsVHDL(std::ostream &stream, unsigned indent)
{
	CodeFormatting &cf = m_ast.getCodeFormatting();

	struct RWPorts {
		std::vector<hlim::MemoryGroup::ReadPort> readPorts;
		std::vector<hlim::MemoryGroup::WritePort> writePorts;
	};
	std::map<hlim::Clock*, RWPorts> clocks;

	for (auto &wp : m_memGrp->getWritePorts())
		clocks[wp.node->getClocks()[0]->getClockPinSource()].writePorts.push_back(wp);

	for (auto &rp : m_memGrp->getReadPorts())
		if (!rp.dedicatedReadLatencyRegisters.empty())
			clocks[rp.dedicatedReadLatencyRegisters.front()->getClocks()[0]->getClockPinSource()].readPorts.push_back(rp);
		else
			clocks[nullptr].readPorts.push_back(rp);

	for (auto clock : clocks) {
		if (clock.first != nullptr) {
			unsigned indent = 1;
			cf.indent(stream, indent);
			stream << "PROCESS("<< m_namespaceScope.getClock(clock.first).name <<")\n";
			cf.indent(stream, indent);
			stream << "BEGIN\n";

			indent++;

				cf.indent(stream, indent);
				switch (clock.first->getTriggerEvent()) {
					case hlim::Clock::TriggerEvent::RISING:
						stream << "IF (rising_edge(" << m_namespaceScope.getClock(clock.first).name << ")) THEN\n";
					break;
					case hlim::Clock::TriggerEvent::FALLING:
						stream << "IF (falling_edge(" << m_namespaceScope.getClock(clock.first).name << ")) THEN\n";
					break;
					case hlim::Clock::TriggerEvent::RISING_AND_FALLING:
						stream << "IF (" << m_namespaceScope.getClock(clock.first).name << "'event) THEN\n";
					break;
					default:
						HCL_ASSERT_HINT(false, "Unhandled case");
				}
				indent++;

				for (auto &wp : clock.second.writePorts) {
					auto enablePort = wp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::enable);
					if (enablePort.node != nullptr) {
						cf.indent(stream, indent);
						stream << "IF ("<< m_namespaceScope.get(enablePort).name << " = '1') THEN\n";
						indent++;
					}


					auto addrPort = wp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::address);
					auto dataPort = wp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::wrData);

					const auto &dataDecl = m_namespaceScope.get(dataPort);

					cf.indent(stream, indent);
					stream << "memory(to_integer(" << m_namespaceScope.get(addrPort).name << ")) <= ";
					if (dataDecl.dataType != VHDLDataType::UNSIGNED) {
						cf.formatDataType(stream, VHDLDataType::UNSIGNED);
						stream << '(' << dataDecl.name << ";\n";
					} else
						stream << dataDecl.name << ";\n";

					if (enablePort.node != nullptr) {
						indent--;
						cf.indent(stream, indent);
						stream << "END IF;\n";
					}
				}
				for (auto &rp : clock.second.readPorts) {
					auto addrPort = rp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::address);
					auto dataPort = rp.dataOutput;

					for (auto i : utils::Range(rp.dedicatedReadLatencyRegisters.size())) {
						auto enablePort = rp.dedicatedReadLatencyRegisters[i]->getDriver((unsigned)hlim::Node_Register::Input::ENABLE); //rp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::enable);
						if (enablePort.node != nullptr) {
							cf.indent(stream, indent);
							stream << "IF ("<< m_namespaceScope.get(enablePort).name << " = '1') THEN\n";
							indent++;
						}

						const auto &outputDecl = m_namespaceScope.get(dataPort);

						cf.indent(stream, indent);
						if (i+1 == rp.dedicatedReadLatencyRegisters.size()) {
							stream << outputDecl.name;
						} else {
							stream << outputDecl.name << "_outputReg_" << i;
						}

						stream << " <= ";

						if (outputDecl.dataType != VHDLDataType::UNSIGNED) {
							cf.formatDataType(stream, outputDecl.dataType);
							stream << '(';
						}

						if (i == 0) {
							stream << "memory(to_integer(" << m_namespaceScope.get(addrPort).name << "));\n";
						} else {
							stream  << outputDecl.name << "_outputReg_" << i-1 << ";\n";
						}

						if (outputDecl.dataType != VHDLDataType::UNSIGNED) {
							stream << ')';
						}


						if (enablePort.node != nullptr) {
							indent--;
							cf.indent(stream, indent);
							stream << "END IF;\n";
						}

						if (clock.first->getRegAttribs().resetType == hlim::RegisterAttributes::ResetType::SYNCHRONOUS) {
							auto reset = rp.dedicatedReadLatencyRegisters[i]->getDriver((unsigned)hlim::Node_Register::Input::RESET_VALUE);
							if (reset.node != nullptr) {
								cf.indent(stream, indent);
								const char resetValue = clock.first->getRegAttribs().resetActive == hlim::RegisterAttributes::Active::HIGH ? '1' : '0';
								stream << "IF ("<< m_namespaceScope.getReset(clock.first).name << " = '" << resetValue << "') THEN\n";
								indent++;

								cf.indent(stream, indent);
								if (i+1 == rp.dedicatedReadLatencyRegisters.size()) {
									stream << m_namespaceScope.get(dataPort).name;
								} else {
									stream << m_namespaceScope.get(dataPort).name << "_outputReg_" << i;
								}
								stream << " <= " << m_namespaceScope.get(reset).name << ";\n";

								indent--;
								cf.indent(stream, indent);
								stream << "END IF;\n";
							}
						}
					}
				}

				indent--;

				cf.indent(stream, indent);
				stream << "END IF;\n";

				if (clock.first->getRegAttribs().resetType == hlim::RegisterAttributes::ResetType::ASYNCHRONOUS) {
					for (auto &rp : clock.second.readPorts) {
						for (auto i : utils::Range(rp.dedicatedReadLatencyRegisters.size())) {
							auto reset = rp.dedicatedReadLatencyRegisters[i]->getDriver((unsigned)hlim::Node_Register::Input::RESET_VALUE);
							if (reset.node != nullptr) {
								cf.indent(stream, indent);
								const char resetValue = clock.first->getRegAttribs().resetActive == hlim::RegisterAttributes::Active::HIGH ? '1' : '0';
								stream << "IF ("<< m_namespaceScope.getReset(clock.first).name << " = '" << resetValue << "') THEN\n";
								indent++;

								cf.indent(stream, indent);
								if (i+1 == rp.dedicatedReadLatencyRegisters.size()) {
									stream << m_namespaceScope.get(rp.dataOutput).name;
								} else {
									stream << m_namespaceScope.get(rp.dataOutput).name << "_outputReg_" << i;
								}
								stream << " <= " << m_namespaceScope.get(reset).name << ";\n";									

								indent--;
								cf.indent(stream, indent);
								stream << "END IF;\n";
							}
						}
					}
				}

			indent--;
			cf.indent(stream, indent);
			stream << "END PROCESS;\n\n";
		} else {
			unsigned indent = 1;
			cf.indent(stream, indent);
			stream << "PROCESS(all)\n";
			cf.indent(stream, indent);
			stream << "BEGIN\n";

			indent++;

				for (auto &rp : clock.second.readPorts) {
					auto enablePort = rp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::enable);
					if (enablePort.node != nullptr) {
						cf.indent(stream, indent);
						stream << "-- Ignoring read enable signal "<< m_namespaceScope.get(enablePort).name << " for asynchronous internal memory.\n";
					}


					auto addrPort = rp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::address);
					auto dataPort = rp.dataOutput;
					const auto &outputDecl = m_namespaceScope.get(dataPort);

					cf.indent(stream, indent);
					stream << outputDecl.name << " <= ";
					
					if (outputDecl.dataType != VHDLDataType::UNSIGNED) {
						cf.formatDataType(stream, outputDecl.dataType);
						stream << '(';
					}

					stream << "memory(to_integer(" << m_namespaceScope.get(addrPort).name << "));\n";

					if (outputDecl.dataType != VHDLDataType::UNSIGNED)
						stream << ')';
				}

			indent--;

			cf.indent(stream, indent);
			stream << "END PROCESS;\n\n";
		}
	}
}




}
