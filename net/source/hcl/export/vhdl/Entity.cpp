#include "Entity.h"

#include "Block.h"

#include "AST.h"
#include "Package.h"

#include "../../hlim/Clock.h"
#include "../../hlim/coreNodes/Node_Pin.h"


#include <memory>

namespace hcl::core::vhdl {

Entity::Entity(AST &ast, const std::string &desiredName, BasicBlock *parent) : BasicBlock(ast, parent, &ast.getNamespaceScope())
{
    m_name = m_ast.getNamespaceScope().allocateEntityName(desiredName);
}

Entity::~Entity()
{

}

void Entity::buildFrom(hlim::NodeGroup *nodeGroup)
{
    HCL_ASSERT(nodeGroup->getGroupType() == hlim::NodeGroup::GroupType::ENTITY);

    m_comment = nodeGroup->getComment();

    NodeGroupInfo grpInfo;
    grpInfo.buildFrom(nodeGroup, false);

    collectInstantiations(nodeGroup, false);

    processifyNodes("default", nodeGroup, false);

    for (auto &subArea : grpInfo.subAreas) {
        NodeGroupInfo areaInfo;
        areaInfo.buildFrom(subArea, false);

        // If there is nothing but logic inside it's a process, otherwise a block
        if (areaInfo.externalNodes.empty() &&
            areaInfo.subEntities.empty() &&
            areaInfo.subAreas.empty()) {

            processifyNodes(subArea->getName(), subArea, true);
        } else {
            // create block
            m_blocks.push_back(std::make_unique<Block>(this, subArea->getName()));
            auto &block = m_blocks.back();
            block->buildFrom(subArea);

            ConcurrentStatement statement;
            statement.type = ConcurrentStatement::TYPE_BLOCK;
            statement.ref.block = block.get();
            statement.sortIdx = 0; /// @todo

            m_statements.push_back(statement);
        }
    }

    std::sort(m_statements.begin(), m_statements.end());
}

void Entity::extractSignals()
{
    BasicBlock::extractSignals();
    for (auto &block : m_blocks) {
        block->extractSignals();
        routeChildIOUpwards(block.get());
    }
}

void Entity::allocateNames()
{
    for (auto &input : m_inputs)
        m_namespaceScope.allocateName(input, findNearestDesiredName(input), CodeFormatting::SIG_ENTITY_INPUT);

    for (auto &output : m_outputs)
        m_namespaceScope.allocateName(output, findNearestDesiredName(output), CodeFormatting::SIG_ENTITY_OUTPUT);

    for (auto &clock : m_inputClocks)
        m_namespaceScope.allocateName(clock, clock->getName());

    for (auto &ioPin : m_ioPins)
        m_namespaceScope.allocateName(ioPin, ioPin->getName());

    BasicBlock::allocateNames();
    for (auto &block : m_blocks)
        block->allocateNames();
}


void Entity::writeLibrariesVHDL(std::ostream &stream)
{
    stream << "LIBRARY ieee;" << std::endl
           << "USE ieee.std_logic_1164.ALL;" << std::endl
           << "USE ieee.numeric_std.all;" << std::endl << std::endl;

    // Import everything for now
    for (const auto &package : m_ast.getPackages())
        package->writeImportStatement(stream);
}

std::vector<std::string> Entity::getPortsVHDL()
{
    CodeFormatting &cf = m_ast.getCodeFormatting();

    std::vector<std::string> portList;
    for (const auto &clk : m_inputClocks) {
        std::stringstream line;
        line << m_namespaceScope.getName(clk) << " : IN STD_LOGIC";
        portList.push_back(line.str());
        if (clk->getResetType() != hlim::Clock::ResetType::NONE) {
            std::stringstream line;
            line << m_namespaceScope.getName(clk)<<clk->getResetName() << " : IN STD_LOGIC";
            portList.push_back(line.str());
        }
    }
    for (auto &ioPin : m_ioPins) {
        bool isInput = !ioPin->getDirectlyDriven(0).empty();
        bool isOutput = ioPin->getNonSignalDriver(0).node != nullptr;

        std::stringstream line;
        line << m_namespaceScope.getName(ioPin) << " : ";
        if (isInput && isOutput) {
            line << "INOUT "; // this will need more thought at other places to work
            cf.formatConnectionType(line, ioPin->getOutputConnectionType(0));
        } else if (isInput) {
            line << "IN ";
            cf.formatConnectionType(line, ioPin->getOutputConnectionType(0));
        } else if (isOutput) {
            line << "OUT ";
            auto driver = ioPin->getNonSignalDriver(0);
            cf.formatConnectionType(line, hlim::getOutputConnectionType(driver));
        } else
            continue;
        portList.push_back(line.str());
    }
    for (const auto &signal : m_inputs) {
        std::stringstream line;
        line << m_namespaceScope.getName(signal) << " : IN ";
        cf.formatConnectionType(line, hlim::getOutputConnectionType(signal));
        portList.push_back(line.str());
    }
    for (const auto &signal : m_outputs) {
        std::stringstream line;
        line << m_namespaceScope.getName(signal) << " : OUT ";
        cf.formatConnectionType(line, hlim::getOutputConnectionType(signal));
        portList.push_back(line.str());
    }
    return portList;
}

void Entity::writeLocalSignalsVHDL(std::ostream &stream)
{
    CodeFormatting &cf = m_ast.getCodeFormatting();

    for (const auto &signal : m_localSignals) {
        cf.indent(stream, 1);
        stream << "SIGNAL " << m_namespaceScope.getName(signal) << " : ";
        cf.formatConnectionType(stream, hlim::getOutputConnectionType(signal));
        stream << "; "<< std::endl;
    }
}



void Entity::writeVHDL(std::ostream &stream)
{
    CodeFormatting &cf = m_ast.getCodeFormatting();

    stream << cf.getFileHeader();

    writeLibrariesVHDL(stream);

    cf.formatEntityComment(stream, m_name, m_comment);

    stream << "ENTITY " << m_name << " IS " << std::endl;
    cf.indent(stream, 1); stream << "PORT(" << std::endl;

    {
        std::vector<std::string> portList = getPortsVHDL();

        for (auto i : utils::Range(portList.size())) {
            cf.indent(stream, 2);
            stream << portList[i];
            if (i+1 < portList.size())
                stream << ";";
            stream << std::endl;
        }
    }

    cf.indent(stream, 1); stream << ");" << std::endl;
    stream << "END " << m_name << ";" << std::endl << std::endl;

    stream << "ARCHITECTURE impl OF " << m_name << " IS " << std::endl;

    writeLocalSignalsVHDL(stream);

    stream << "BEGIN" << std::endl;

    writeStatementsVHDL(stream, 1);

    stream << "END impl;" << std::endl;
}

void Entity::writeInstantiationVHDL(std::ostream &stream, unsigned indent, const std::string &instanceName)
{
    CodeFormatting &cf = m_ast.getCodeFormatting();

    cf.indent(stream, indent);
    stream << instanceName << " : entity work." << getName() << "(impl) port map (" << std::endl;

    std::vector<std::string> portmapList;


    for (auto &s : m_inputClocks) {
        std::stringstream line;
        line << m_namespaceScope.getName(s) << " => ";
        line << m_parent->getNamespaceScope().getName(s);
        portmapList.push_back(line.str());
        if (s->getResetType() != hlim::Clock::ResetType::NONE) {
            std::stringstream line;
            line << m_namespaceScope.getName(s)<<s->getResetName() << " => ";
            line << m_parent->getNamespaceScope().getName(s)<<s->getResetName();
            portmapList.push_back(line.str());
        }
    }
    for (auto &s : m_ioPins) {
        std::stringstream line;
        line << m_namespaceScope.getName(s) << " => ";
        line << m_parent->getNamespaceScope().getName(s);
        portmapList.push_back(line.str());
    }
    for (auto &s : m_inputs) {
        std::stringstream line;
        line << m_namespaceScope.getName(s) << " => ";
        line << m_parent->getNamespaceScope().getName(s);
        portmapList.push_back(line.str());
    }
    for (auto &s : m_outputs) {
        std::stringstream line;
        line << m_namespaceScope.getName(s) << " => ";
        line << m_parent->getNamespaceScope().getName(s);
        portmapList.push_back(line.str());
    }

    for (auto i : utils::Range(portmapList.size())) {
        cf.indent(stream, indent+1);
        stream << portmapList[i];
        if (i+1 < portmapList.size())
            stream << ",";
        stream << std::endl;
    }


    cf.indent(stream, indent);
    stream << ");" << std::endl;
}


Entity *Entity::getParentEntity()
{
    auto *parent = getParent();
    while (parent != nullptr) {
        if (auto *entity = dynamic_cast<Entity*>(parent))
            return entity;
        else
            parent = parent->getParent();
    }
    return nullptr;
}

}
