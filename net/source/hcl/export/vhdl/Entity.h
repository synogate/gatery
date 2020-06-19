#pragma once

#include "BasicBlock.h"

#include <string>
#include <vector>
#include <memory>
#include <ostream>

namespace hcl::core::vhdl {

class Block;
    
/**
 * @todo write docs
 */
class Entity : public BasicBlock
{
    public:
        Entity(AST &ast, const std::string &desiredName, BasicBlock *parent);
        virtual ~Entity();

        inline const std::string &getName() const { return m_name; }
        
        void buildFrom(hlim::NodeGroup *nodeGroup);
        
        virtual void extractSignals() override;
        virtual void allocateNames() override;
        
        void writeVHDL(std::ostream &stream);
    protected:        
        std::vector<std::unique_ptr<Block>> m_blocks;
};

}
