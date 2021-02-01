#include "GenericMemoryEntity.h"

#include "AST.h"

#include "../../hlim/Clock.h"
#include "../../hlim/coreNodes/Node_Register.h"
#include "../../hlim/supportNodes/Node_Memory.h"
#include "../../hlim/supportNodes/Node_MemPort.h"
#include "../../hlim/MemoryDetector.h"


#include <map>
#include <vector>

namespace hcl::core::vhdl {

GenericMemoryEntity::GenericMemoryEntity(AST &ast, const std::string &desiredName, BasicBlock *parent) : Entity(ast, desiredName, parent)
{
}

void GenericMemoryEntity::buildFrom(hlim::MemoryGroup *memGrp)
{
    m_memGrp = memGrp;
    // probably not the best place to do it....
    m_namespaceScope.allocateName({.node=memGrp->getMemory(), .port=0}, "memory", CodeFormatting::SIG_LOCAL_SIGNAL);
    
    for (auto node : memGrp->getNodes())
        m_ast.getMapping().assignNodeToScope(node, this);

    for (auto &wp : memGrp->getWritePorts()) {
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

        m_inputClocks.insert(wp.node->getClocks()[0]);
    }
    for (auto &rp : memGrp->getReadPorts()) {
        auto addrInput = rp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::address);
        auto enInput = rp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::enable);
        auto dataOutput = rp.dataOutput;

        if (addrInput.node != nullptr)
            m_inputs.insert(addrInput);
        if (enInput.node != nullptr)
            m_inputs.insert(enInput);

        m_outputs.insert(dataOutput);


        if (rp.syncReadDataReg != nullptr)
            m_inputClocks.insert(rp.syncReadDataReg->getClocks()[0]);

        if (rp.outputReg != nullptr) {
            m_inputClocks.insert(rp.outputReg->getClocks()[0]);

            auto enInput = rp.outputReg->getDriver((unsigned)hlim::Node_Register::Input::ENABLE);
            if (enInput.node != nullptr)
                m_inputs.insert(enInput);

            auto resetValue = rp.outputReg->getDriver((unsigned)hlim::Node_Register::Input::RESET_VALUE);
            if (resetValue.node != nullptr)
                m_inputs.insert(resetValue);
        }
    }
}

void GenericMemoryEntity::writeLocalSignalsVHDL(std::ostream &stream) 
{
    CodeFormatting &cf = m_ast.getCodeFormatting();

    Entity::writeLocalSignalsVHDL(stream);

    auto memorySize = m_memGrp->getMemory()->getSize();

    std::set<size_t> portSizes;

    for (auto &wp : m_memGrp->getWritePorts())
        portSizes.insert(wp.node->getBitWidth());

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
    stream << "SIGNAL memory : mem_type;\n";


    for (auto &rp : m_memGrp->getReadPorts())
        if (rp.outputReg != nullptr) {
            cf.indent(stream, 1);
            stream << "SIGNAL " << m_namespaceScope.getName(rp.dataOutput) << "_outputReg : ";
            cf.formatConnectionType(stream, rp.dataOutput.node->getOutputConnectionType(rp.dataOutput.port));
            stream << "; "<< std::endl;
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
        clocks[wp.node->getClocks()[0]].writePorts.push_back(wp);

    for (auto &rp : m_memGrp->getReadPorts())
        if (rp.syncReadDataReg != nullptr)
            clocks[rp.syncReadDataReg->getClocks()[0]].readPorts.push_back(rp);
        else
            clocks[nullptr].readPorts.push_back(rp);
    
    // If ROM, initialize in combinatorial process
    if (m_memGrp->getWritePorts().empty()) {

        auto memorySize = m_memGrp->getMemory()->getSize();

        std::set<size_t> portSizes;

        for (auto &wp : m_memGrp->getWritePorts())
            portSizes.insert(wp.node->getBitWidth());

        for (auto &rp : m_memGrp->getReadPorts())
            portSizes.insert(rp.node->getBitWidth());

        HCL_ASSERT_HINT(portSizes.size() == 1, "Memory with mixed port sizes not yet supported!");

        size_t wordSize = *portSizes.begin();

        HCL_ASSERT_HINT(memorySize % wordSize == 0, "Memory size is not a multiple of the word size!");

        const auto &powerOnState = m_memGrp->getMemory()->getPowerOnState();
        unsigned indent = 1;

        for (auto i : utils::Range(memorySize/wordSize)) {
            cf.indent(stream, indent);
            stream << "memory("<<i<<") <= \"";
            for (auto j : utils::Range(wordSize)) {
                bool defined = powerOnState.get(hcl::core::sim::DefaultConfig::DEFINED, i*wordSize + wordSize-1-j);
                bool value = powerOnState.get(hcl::core::sim::DefaultConfig::VALUE, i*wordSize + wordSize-1-j);
                if (!defined)
                    stream << 'x';
                else 
                    if (value)
                        stream << '1';
                    else
                        stream << '0';
            }
            stream << "\";\n";
        }
        stream << "\n";
    }

    for (auto clock : clocks) {
        if (clock.first != nullptr) {
            unsigned indent = 1;
            cf.indent(stream, indent);
            stream << "PROCESS("<< m_namespaceScope.getName(clock.first) <<")\n";
            cf.indent(stream, indent);
            stream << "BEGIN\n";

            indent++;

                cf.indent(stream, indent);
                switch (clock.first->getTriggerEvent()) {
                    case hlim::Clock::TriggerEvent::RISING: 
                        stream << "IF (rising_edge(" << m_namespaceScope.getName(clock.first) << ")) THEN\n"; 
                    break;
                    case hlim::Clock::TriggerEvent::FALLING: 
                        stream << "IF (falling_edge(" << m_namespaceScope.getName(clock.first) << ")) THEN\n"; 
                    break;
                    case hlim::Clock::TriggerEvent::RISING_AND_FALLING: 
                        stream << "IF (" << m_namespaceScope.getName(clock.first) << "'event) THEN\n"; 
                    break;
                    default:
                        HCL_ASSERT_HINT(false, "Unhandled case"); 
                }
                indent++;

                for (auto &wp : clock.second.writePorts) {
                    auto enablePort = wp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::enable);
                    if (enablePort.node != nullptr) {
                        cf.indent(stream, indent);
                        stream << "IF ("<< m_namespaceScope.getName(enablePort) << " = '1') THEN\n"; 
                        indent++;
                    }


                    auto addrPort = wp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::address);
                    auto dataPort = wp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::wrData);

                    cf.indent(stream, indent);
                    stream << "memory(to_integer(" << m_namespaceScope.getName(addrPort) << ")) <= " << m_namespaceScope.getName(dataPort) << ";\n";

                    if (enablePort.node != nullptr) {
                        indent--;
                        cf.indent(stream, indent);
                        stream << "END IF;\n"; 
                    }
                }
                for (auto &rp : clock.second.readPorts) {
                    auto enablePort = rp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::enable);
                    if (enablePort.node != nullptr) {
                        cf.indent(stream, indent);
                        stream << "IF ("<< m_namespaceScope.getName(enablePort) << " = '1') THEN\n"; 
                        indent++;
                    }


                    auto addrPort = rp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::address);
                    auto dataPort = rp.dataOutput;

                    cf.indent(stream, indent);
                    if (rp.outputReg == nullptr) {
                        stream << m_namespaceScope.getName(dataPort);
                    } else {
                        stream << m_namespaceScope.getName(dataPort) << "_outputReg";
                    }
                    stream << " <= memory(to_integer(" << m_namespaceScope.getName(addrPort) << "));\n";

                    if (enablePort.node != nullptr) {
                        indent--;
                        cf.indent(stream, indent);
                        stream << "END IF;\n"; 
                    }

                    if (rp.outputReg != nullptr) {
                        auto enablePort = rp.outputReg->getDriver((unsigned)hlim::Node_Register::Input::ENABLE);
                        if (enablePort.node != nullptr) {
                            cf.indent(stream, indent);
                            stream << "IF ("<< m_namespaceScope.getName(enablePort) << " = '1') THEN\n"; 
                            indent++;
                        }
                        cf.indent(stream, indent);
                        stream << m_namespaceScope.getName(dataPort) << " <= " << m_namespaceScope.getName(dataPort) << "_outputReg;\n";

                        if (enablePort.node != nullptr) {
                            indent--;
                            cf.indent(stream, indent);
                            stream << "END IF;\n"; 
                        }

                        if (clock.first->getResetType() == hlim::Clock::ResetType::SYNCHRONOUS) {
                            auto reset = rp.outputReg->getDriver((unsigned)hlim::Node_Register::Input::RESET_VALUE);
                            if (reset.node != nullptr) {
                                cf.indent(stream, indent);
                                stream << "IF ("<< m_namespaceScope.getName(clock.first)<<clock.first->getResetName() << " = '1') THEN\n"; 
                                indent++;

                                    cf.indent(stream, indent);
                                    stream << m_namespaceScope.getName(dataPort) << " <= " << m_namespaceScope.getName(reset) << ";\n";

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

                if (clock.first->getResetType() == hlim::Clock::ResetType::ASYNCHRONOUS) {
                    for (auto &rp : clock.second.readPorts) {
                        if (rp.outputReg != nullptr) {
                            auto reset = rp.outputReg->getDriver((unsigned)hlim::Node_Register::Input::RESET_VALUE);
                            if (reset.node != nullptr) {
                                cf.indent(stream, indent);
                                stream << "IF ("<< m_namespaceScope.getName(clock.first)<<clock.first->getResetName() << " = '1') THEN\n"; 
                                indent++;

                                    cf.indent(stream, indent);
                                    stream << m_namespaceScope.getName(rp.dataOutput) << " <= " << m_namespaceScope.getName(reset) << ";\n";

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
                        stream << "IF ("<< m_namespaceScope.getName(enablePort) << " = '1') THEN\n"; 
                        indent++;
                    }


                    auto addrPort = rp.node->getDriver((unsigned)hlim::Node_MemPort::Inputs::address);
                    auto dataPort = rp.dataOutput;

                    cf.indent(stream, indent);
                    stream << m_namespaceScope.getName(dataPort) << " <= memory(to_integer(" << m_namespaceScope.getName(addrPort) << "));\n";

                    if (enablePort.node != nullptr) {
                        indent--;
                        cf.indent(stream, indent);
                        stream << "END IF;\n"; 
                    }
                }

            indent--;

            cf.indent(stream, indent);
            stream << "END PROCESS;\n\n";
        }
    }
}




}