#include "Entity.h"

#include "Block.h"

#include "AST.h"

#include "../../hlim/Clock.h"


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

    BasicBlock::allocateNames();
    for (auto &block : m_blocks)
        block->allocateNames();
}


void Entity::writeVHDL(std::ostream &stream)
{
    CodeFormatting &cf = m_ast.getCodeFormatting();
    
    stream << cf.getFileHeader();
    
    stream << "LIBRARY ieee;" << std::endl 
           << "USE ieee.std_logic_1164.ALL;" << std::endl 
           << "USE ieee.numeric_std.all;" << std::endl << std::endl;
    
    
    cf.formatEntityComment(stream, m_name, m_comment);

    stream << "ENTITY " << m_name << " IS " << std::endl;
    cf.indent(stream, 1); stream << "PORT(" << std::endl;
    
    {
        std::vector<std::string> portList;

        portList.push_back("reset : IN STD_LOGIC");
    
        for (const auto &clk : m_inputClocks) {
            std::stringstream line;
            line << m_namespaceScope.getName(clk) << " : IN STD_LOGIC";
            portList.push_back(line.str());
        }
        for (const auto &signal : m_inputs) {
            std::stringstream line;
            line << m_namespaceScope.getName(signal) << " : IN ";
            cf.formatConnectionType(line, signal.node->getOutputConnectionType(signal.port));
            portList.push_back(line.str());
        }
        for (const auto &signal : m_outputs) {
            std::stringstream line;
            line << m_namespaceScope.getName(signal) << " : OUT ";
            cf.formatConnectionType(line, signal.node->getOutputConnectionType(signal.port));
            portList.push_back(line.str());
        }
        
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
    
    for (const auto &signal : m_localSignals) {
        cf.indent(stream, 1);
        stream << "SIGNAL " << m_namespaceScope.getName(signal) << " : ";
        cf.formatConnectionType(stream, signal.node->getOutputConnectionType(signal.port));
        stream << "; "<< std::endl;
    }        
    
    stream << "BEGIN" << std::endl;
    
    writeStatementsVHDL(stream, 1);

    stream << "END impl;" << std::endl;

}

   
}
